# CORBA spec gap analysis

What the implementation has versus what the OMG CORBA specification
requires, and — for everything missing — whether it's actually
implementable given a UDP transport instead of TCP/IIOP.

**Baseline**: this project already committed (see
`docs/standards/spec-references.md`) to the **CORBA/e Compact Profile**
subset of CORBA 3.4, not full CORBA. That means DII, DSI, and the
Interface Repository are *out of scope by design*, not gaps — they're
listed below for completeness, marked accordingly, and their absence has
nothing to do with UDP.

**Status as of Phase 1 completion** (see `docs/roadmap.md` and
`docs/architecture/adr-0002-giop-framing.md`): CDR is complete for this
project's needs and the full GIOP message set is encodable/decodable.
Nothing below that's now built is re-evaluated for UDP-feasibility,
since it's done — the remaining open items are all Phase 2+ (the actual
UDP transport) or Phase 3+ (ORB/POA) territory.

## 1. CDR (Common Data Representation) — formal/12-11-14 §9

| Item | Status |
|---|---|
| Primitive types (octet, boolean, u/i16/32/64, f32/f64) | **Done** — `runtime/src/cdr/cdr.c`, alignment + byte-order swap included. |
| Byte-order bootstrap / encapsulation-style framing | **Done** — via two general primitives rather than one dedicated "encapsulation" function: `hace_cdr_buffer_set_byte_order` (switch a buffer's byte order mid-stream, after reading a leading order flag) and `hace_cdr_buffer_init_view` (bind a bounded, non-copying sub-buffer). Together they're what `hace_giop_decode_message_header` uses to solve GIOP's byte-order bootstrap problem, and what any future IOR/TypeCode encapsulation (Phase 3) would reuse. |
| Strings, `sequence<T>` length framing, bulk `sequence<octet>` | **Done** — `hace_cdr_encode_string`/`decode_string`, `hace_cdr_encode_sequence_length`/`decode_sequence_length`, `hace_cdr_encode_octet_seq`/`decode_octet_seq`. All bounded (explicit max-length/max-count, no dynamic allocation; a decoded length/count beyond the caller's bound is rejected outright, never truncated). |
| Structs, arrays, unions, enums | **Done, with no dedicated API** — CDR itself has none: a struct is its members encoded in order, an array is its elements in order, a union is its discriminant then the active member, an enum is a `u32`. `runtime/tests/test_cdr.c` has a worked example of the struct and union patterns. |
| wstring/wchar (wide characters) | **Deferred, not built.** UDP-feasible (trivial additional encoding rule), but low value for avionics data; add only if a concrete IDL need shows up. |
| `any` / TypeCode / DynAny | **Excluded by design.** Minimum CORBA/CORBA-e Compact Profile drop dynamic typing support; a statically-typed embedded ORB has no use for runtime type introspection. Unrelated to UDP. |
| valuetype (OO pass-by-value types, CORBA 2.3+) | **Excluded by design.** Heavyweight, essentially unused outside enterprise Java/C++ CORBA; not worth the certification surface. Unrelated to UDP. |

## 2. GIOP message protocol — formal/12-11-14 §7

| Message | Status |
|---|---|
| Request, Reply | **Done** (encode/decode only — `runtime/include/hace/giop.h`). Actually sending/receiving them over UDP is Phase 2. |
| LocateRequest, LocateReply | **Done** (encode/decode only). |
| CancelRequest | **Done** (encode/decode only). Over UDP this is inherently best-effort (the target may already have replied, or the cancel datagram may itself be lost) — arguably a more *honest* fit for CancelRequest's already-advisory semantics than TCP gives it. |
| MessageError | **Done** — needs no per-type function at all: `hace_giop_begin_message` + `hace_giop_finish_message(..., HACE_GIOP_MSG_MESSAGE_ERROR, ...)` with nothing encoded in between already produces a valid zero-body MessageError. |
| Fragment (header only) | **Done** (header — `request_id` — only). The actual "split a message across N datagrams and reassemble" state machine is Phase 2, a transport concern, not a wire-format one. |
| **CloseConnection** | **N/A — not implementable over UDP, by nature of the message, not a limitation of this project.** CloseConnection exists to signal that a GIOP implementation is tearing down its underlying TCP connection. UDP has no connection to close; there is nothing for this message to mean. `hace_giop_message_type_t` still numbers it correctly for spec-fidelity, and decoding one isn't itself an error, but nothing in this project emits or acts on it — see `docs/architecture/adr-0002-giop-framing.md`. |

## 3. IIOP / IOR — formal/12-11-14 §7.6, §9

| Item | Status |
|---|---|
| IIOP transport profile | **Superseded by design** — this project's whole premise is UIOP (GIOP-over-UDP) instead of IIOP. Not a gap. |
| IOR encoding (object reference: host, port, object key) | **Missing — UDP-feasible (planned, Phase 3).** A UIOP profile tag replaces the IIOP profile tag; otherwise unchanged. |
| Interoperable Naming (`corbaloc:`/`corbaname:` URLs) | **Missing — UDP-feasible.** Needs a `uiop:` URL scheme instead of `iiop:`; otherwise a direct analog. Not started; candidate for an optional later phase (§7 below). |

## 4. ORB core interface — formal/21-02-02 §4

| Item | Status |
|---|---|
| `ORB_init`, `resolve_initial_references`, `object_to_string`/`string_to_object`, `shutdown` | **Missing — UDP-feasible (planned, Phase 3).** Entirely transport-independent; this is basic ORB bootstrap that has to exist no matter what. |
| System exceptions (`COMM_FAILURE`, `TIMEOUT`, `TRANSIENT`, `OBJECT_NOT_EXIST`, etc.) and the user-exception mechanism | **Missing — UDP-feasible (planned, Phase 3).** The GIOP layer built in Phase 1 already has `hace_giop_status_t` for wire/framing errors and `hace_giop_reply_status_t` for `USER_EXCEPTION`/`SYSTEM_EXCEPTION` signaling, but there is no CORBA-level exception *model* (the actual system exception types/minor codes/completion status) yet. Several system exceptions map naturally onto UDP failure modes: a lost/retried datagram becomes `TRANSIENT` or `TIMEOUT`, not `COMM_FAILURE` (which in IIOP usually means "the TCP connection died," a concept UDP doesn't have). |

## 5. Portable Object Adapter (POA) — formal/21-02-02 §11

| Item | Status |
|---|---|
| Static object activation, basic POA policies (compatible with CORBA/e Compact Profile: no DII/DSI-dependent policies) | **Missing — UDP-feasible (planned, Phase 3).** Object dispatch logic is transport-independent. |
| Dynamic activation (`USE_SERVANT_MANAGER`, etc.), full POA policy set | **Excluded by design** — Compact Profile scope decision already made, not a UDP question. |

## 6. Real-Time CORBA — formal/05-01-04

| Item | Status |
|---|---|
| Thread pools, priority propagation from client to server thread | **Missing — UDP-feasible (planned, Phase 3, borrowing the concepts as already noted in `docs/standards/spec-references.md`).** |
| Priority-banded connections, private connections (RT-CORBA §4) | **N/A — not implementable over UDP.** These policies exist specifically to manage *multiple persistent TCP connections* at different priority bands. UDP has no connections to band or privatize; there's nothing underneath these policies once the transport is connectionless. |

## 7. Messaging / AMI — formal/21-02-02 §22 (Messaging)

| Item | Status |
|---|---|
| Poller-based asynchronous invocation (send now, poll/await reply later) | **Missing — UDP-feasible (planned, Phase 5 `async_ami` example).** A good fit: UDP send is inherently non-blocking/fire-and-forget at the transport level already. |
| `SYNC_WITH_TRANSPORT` / `SYNC_WITH_SERVER` / `SYNC_WITH_TARGET` QoS policies | **Decided, built.** `hace_giop_request_header_t.response_expected` already collapses this to oneway vs. `SYNC_WITH_TARGET`-equivalent (wait for the actual application reply); the GIOP decode functions reject `SYNC_WITH_TRANSPORT`/`SYNC_WITH_SERVER` outright (`HACE_GIOP_ERR_UNSUPPORTED_SYNC_SCOPE`) rather than silently reinterpreting them, since neither has a clean UDP equivalent — UDP gives no delivery acknowledgment at all, so "transport accepted it" isn't a meaningful synchronization point. See `docs/architecture/adr-0002-giop-framing.md`. |
| `ORDER_TEMPORAL` / `ORDER_DEADLINE` ordering policies | **Missing — UDP-feasible, but needs extra work.** UDP can reorder datagrams in flight (TCP can't, within one connection), so temporal ordering isn't free the way it is over IIOP — it requires sequence numbers plus a small reorder buffer at the UIOP layer (Phase 2). Optional/stretch, not in the near-term plan. |

## 8. Bidirectional GIOP — formal/12-11-14 §7.15 (BiDirIIOP)

| Item | Status |
|---|---|
| Callback support (server calls back into the client without the client running its own listener socket) | **Missing — UDP-feasible, and easier than over TCP.** BiDirIIOP exists because IIOP connections are normally client→server only; using one connection in both directions needs an explicit negotiation (a `BiDirIIOPServiceContext`). UDP sockets have no inherent direction restriction — once each side knows the other's address/port, either can send. The `observer_attach` example (Phase 5) needs *some* form of callback, but over UIOP it likely doesn't need the BiDirIIOP negotiation machinery at all, just two ordinary UIOP object references pointing at each other. |

## 9. Naming and directory services (CosNaming, not part of CORBA core)

| Item | Status |
|---|---|
| Naming Service (`CosNaming`) | **Missing — UDP-feasible, optional.** It's an ordinary CORBA service once an ORB exists — a naming context object with `bind`/`resolve` operations — nothing UDP-specific about it. Not in the near-term plan; candidate optional phase after Phase 3. |

## 10. Portable Interceptors — formal/21-02-02 §21

| Item | Status |
|---|---|
| Client/server/IOR request interceptors | **Missing — UDP-feasible, but not recommended.** Transport-independent (interceptors operate on the in-process request pipeline, not the wire), so nothing about UDP blocks this. Recommendation: skip, for the same reason DII/DSI were dropped — dynamic behavior injection at the request pipeline works against static analyzability, which is exactly what a DAL C codebase wants to maximize. |

## 11. Out of scope regardless of transport

These aren't evaluated for UDP-feasibility because they were already excluded on scope grounds (heavyweight optional OMG services with no place in a minimal embedded DAL C ORB), matching the CORBA Components exclusion already recorded in `docs/standards/spec-references.md`:

- CORBA Security Service
- CORBA Transaction Service (OTS)
- CosNotification / CosEvent (Notification and Event services)
- CosLifeCycle, CosTrading
- Fault-Tolerant CORBA (FT-CORBA), load balancing
- CORBA Components (already excluded)

## Implementation plan (UDP-feasible gaps only)

Everything here folds into the existing `docs/roadmap.md` phases rather
than creating a parallel plan — this is that roadmap made concrete about
*why* each item is in or out.

**Phase 1 (CDR + GIOP core) — done.** CDR completion (encapsulation-style
byte-order bootstrap, strings, sequences, octet sequences; structs/
unions/arrays/enums needing no dedicated API) and the full GIOP message
set's encode/decode (Request/Reply/CancelRequest/LocateRequest/
LocateReply/MessageError/Fragment-header), with `CloseConnection`
explicitly documented as N/A rather than left as an open "TODO" — see
`docs/architecture/adr-0002-giop-framing.md`.

**Phase 2 (UIOP transport)**
1. Send/receive the Phase 1 GIOP messages over actual UDP sockets.
2. Request-id tracking, bounded retry/timeout for twoway calls.
3. Real fragmentation: splitting an oversized message across a bounded
   number of Fragment datagrams and reassembling them on receipt.
4. Bidirectional object references for callbacks (no BiDirIIOP-style
   negotiation needed — see §8 above).

**Phase 3 (ORB core / POA)**
1. `ORB_init`, `resolve_initial_references`, `object_to_string`/
   `string_to_object` (with a `uiop:` URL scheme), `shutdown`.
2. CORBA exception model: system exceptions mapped onto UDP-realistic
   failure modes (`TRANSIENT`/`TIMEOUT` for lost-and-retried datagrams,
   `OBJECT_NOT_EXIST`, etc. — deliberately *not* modeling
   `COMM_FAILURE` as a first-class case the way IIOP does, since there's
   no connection to fail).
3. Static POA (object activation, dispatch) — Compact Profile subset,
   as already scoped.
4. IOR encoding with a UIOP profile tag.
5. RT-CORBA-inspired thread pool + priority propagation (not
   priority-banded connections — see §6, N/A over UDP).

**Optional / later, not in the near-term plan**
- Naming Service (§9) — straightforward once Phase 3 lands, but not
  blocking anything else.
- `ORDER_TEMPORAL`/`ORDER_DEADLINE` sequencing (§7) — only if a concrete
  use case needs strict ordering; adds a reorder buffer to UIOP.
- Portable Interceptors (§10) — recommended against, not just deferred;
  revisit only if a specific requirement demands it.

## Summary

Almost everything missing is missing simply because it hasn't been
built yet (Phases 2-3 of the existing roadmap), not because UDP makes it
impossible. The only spec-level casualties of choosing UDP are:
**GIOP CloseConnection** (no connection to close) and **RT-CORBA
priority-banded/private connections** (no connections to band) — both
concepts that only exist because IIOP runs over a persistent stream.
Everything else — CDR, the rest of GIOP, the ORB interface, exceptions,
the POA, AMI/Poller async, and even bidirectional callbacks — is fully
buildable over UDP, and in a couple of cases (CancelRequest's
already-advisory semantics, bidirectional callbacks) UDP's
connectionless nature is arguably a *better* fit than TCP's.
