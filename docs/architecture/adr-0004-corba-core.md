# ADR-0004: minimal ORB core — exceptions, IOR, POA/dispatch

## Status

Accepted.

## Context

Phases 1-2 built GIOP message encode/decode and the UIOP UDP transport,
but there was still no notion of a CORBA *object*: no exception model,
no object references, nothing that looked up an object key and
dispatched to a handler. This phase — "a minimal ORB/POA" per
`docs/roadmap.md` — adds the exception model
(`runtime/include/hace/corba_exception.h`), object references
(`corba_ior.h`), and a static Compact-Profile POA with object
activation/dispatch and a bounded worker-task pool (`corba_poa.h`).

Per `docs/corba-spec-gap-analysis.md` §4-6, this was the last of the
genuinely "missing but UDP-feasible" Phase 3 items.

## Decisions

**No separate "ORB" module/handle.** The gap-analysis wishlist listed
`ORB_init`/`resolve_initial_references`/`shutdown` as Phase 3 items.
Concretely: nothing in this minimal profile needs ORB-level state
distinct from a HAL socket + a POA, there's no naming service or second
initial reference to make `resolve_initial_references` meaningful, and
`object_to_string`/`string_to_object` are pure functions on an IOR
struct that don't need an ORB handle at all. Building an empty wrapper
layer just to have three functions exist would be the kind of
speculative API this project has avoided throughout — an explicit,
deliberate scope reduction, not a silently dropped item.

**Exception wire format is three plain `u32`s, not the repository-ID-
string convention.** Real GIOP SystemException bodies carry a string
repository ID (e.g. `"IDL:omg.org/CORBA/OBJECT_NOT_EXIST:1.0"`) for
cross-ORB extensibility. This project isn't an interop target (same
reasoning as ADR-0002's service-context/addressing simplifications), so
`hace_corba_system_exception_t` encodes as exception code + minor code
+ completion status, nothing more.

**A chosen subset of 9 system exceptions**, not CORBA's full ~34:
`UNKNOWN`, `BAD_PARAM`, `NO_MEMORY`, `OBJECT_NOT_EXIST`, `BAD_OPERATION`,
`TRANSIENT`, `TIMEOUT`, `MARSHAL`, `COMM_FAILURE` — enough to describe
every failure this runtime can actually produce. Each carries a
`completion_status` (`YES`/`NO`/`MAYBE`) per the real SystemException
convention, letting a caller judge whether a retry is safe.
`hace_corba_exception_from_uiop_status` maps a failed `hace_uiop_call`
onto one of these so client code doesn't invent its own mapping.

**IOR stringification is hand-rolled, not `snprintf`/`sscanf`.** Many
avionics coding standards restrict or prohibit the C stdio/variadic
printf family in flight code. Consistent with every prior module's
avoidance of libc beyond `memcpy`/`strlen`/`strcmp`,
`object_to_string`/`string_to_object` use small hand-written IPv4
dotted-decimal and hex encode/decode helpers (`runtime/src/corba/corba_ior.c`).
The scheme is corbaloc-style — `uiop:a.b.c.d:port/hexkey` — rather than
the classic hex-dump-the-whole-IOR `"IOR:"` convention real ORBs use,
since this project has no interop requirement with ORBs expecting that
form and the corbaloc-style string is simpler to read and test.

**POA object activation is init-time-only — deliberately no mutex.**
`hace_corba_poa_activate_object` must be called before any worker task
starts dispatching; once dispatch begins, the servant table is
read-only, so concurrent *reads* from multiple worker tasks need no
locking in C. This is simpler than the mutex the HAL's socket/task
tables needed (Phase 0) — those see genuine concurrent *writes*; the
POA table, by construction, never does once serving starts.

**"Multi-session" dispatch needed no new session-tracking structure.**
`hace_uiop_receive` (Phase 2) already does its own self-contained
fragment reassembly per call, entirely on the calling task's stack.
Running N worker tasks (`hace_hal_task_create`, each at its own
priority — the "RT-CORBA-inspired priority mapping" the roadmap calls
for) that each independently call `hace_corba_poa_run` against the same
socket achieves concurrent multi-client handling for free: the OS
delivers each datagram to exactly one waiting `recvfrom()`. No new
multiplexing layer was needed — see the next item for what *did* need
fixing to make that actually true, though.

## A real concurrency bug found and fixed along the way

`hace_uiop_receive` (Phase 2) only surfaced `request_id` from a decoded
Request — not the object key or operation name a POA dispatcher
obviously needs. Fixed by adding `hace_uiop_request_info_t` and a
trailing `out_request_info` parameter to `hace_uiop_receive`, populated
whenever `expected_type == HACE_GIOP_MSG_REQUEST`. Same category of gap
as Phase 2's missing sender address — caught while actually trying to
use the function for its intended purpose, fixed before shipping,
rather than left as a known limitation.

More seriously: `test_corba_poa.c`'s concurrent-dispatch test (three
worker tasks, four concurrent client calls) failed intermittently
(roughly 40% of runs) during development. The cause was in
`hal_linux.c`'s `hace_hal_socket_recv`, not in the new POA code: it used
a two-syscall peek-then-read pattern (`MSG_PEEK|MSG_TRUNC` to learn a
datagram's size, then a second `recvfrom` to consume it) to implement
the documented "reject an oversized datagram rather than truncate it"
contract. That pattern has a window between the two calls. Under
concurrent callers on the same socket — exactly what the worker-task
pool does, and exactly the scenario this phase's whole "no new
multiplexing layer" design rested on being safe — one task's peek could
describe a datagram that a *different* task's consuming read then took
first, leaving the first task's second call to silently consume and
misreport whatever different-sized datagram arrived next (the kernel
truncates into the buffer up to `capacity` without error unless
`MSG_TRUNC` is passed on *that* call too, which the old code didn't).

Fixed by collapsing the two calls into one: `recvfrom(..., MSG_TRUNC |
...)` directly on the real buffer. `MSG_TRUNC` makes a single UDP
`recvfrom` report the datagram's true size even when it's larger than
the buffer (while still only writing `capacity` bytes), so the
oversized-rejection contract is preserved with no window at all — one
atomic syscall per logical receive, correct under any number of
concurrent callers. This also simplified `hal_linux.c` (removed the
peek step, the discard-oversized-leftover hack, and the "peeked
datagram consumed by someone else" `EAGAIN` special case the old code
had partially — but only partially — accounted for).

Regression coverage: `runtime/tests/test_hal.c`'s
`test_concurrent_recv_is_race_free` (four tasks blocked in `recv`
concurrently while four differently-sized, self-describing datagrams
arrive) exercises this directly at the HAL layer; `test_corba_poa.c`'s
`test_poa_concurrent_dispatch` serves as an end-to-end check that the
scenario this phase actually cares about — several worker tasks, one
socket — is solid (verified clean across 20 consecutive runs after the
fix, versus roughly 40% failure before it).

## Consequences

- Dispatch return values are split by layer on purpose:
  `hace_corba_poa_handle_one_request`/`hace_corba_poa_run` return
  `hace_uiop_status_t` (dispatch *mechanics* — did a message arrive,
  did the reply send), not `hace_corba_status_t`. A CORBA-level failure
  (unknown object, servant exception) never reaches that return value —
  it's encoded into the Reply and travels to the actual RPC client,
  which is the only party that should see it.
- Phase 4 (`idlc`) can target `hace_corba_servant_fn_t` as the shape
  generated server skeletons fill in, once it exists.
- Phase 5's examples now have real object activation, real dispatch,
  and real exceptions to build RPC/oneway/async/observer demonstrations
  on top of — none of which existed before this phase.
