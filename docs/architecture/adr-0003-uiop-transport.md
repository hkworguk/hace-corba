# ADR-0003: UIOP transport — fragmentation, retry, and testability

## Status

Accepted.

## Context

Phase 1 built GIOP message encode/decode (`runtime/include/hace/giop.h`)
as pure functions over a caller-supplied buffer — no sockets involved.
Phase 2 needed the actual transport: sending/receiving those messages
over the HAL's UDP sockets (`runtime/include/hace/hal.h`), bounded
retry/timeout for twoway calls, and real fragmentation — splitting an
oversized message across multiple datagrams on send and reassembling it
on receive. `runtime/include/hace/uiop.h` / `runtime/src/uiop/uiop.c`
is that layer.

Scope boundary: this is the transport for a single outstanding call at
a time — a client sending one request and waiting for its one reply.
A server dispatch loop handling many concurrent clients/requests at
once is Phase 3 (ORB/POA), which doesn't exist yet. `hace_uiop_receive`
is written generically enough (parameterized by expected message type
and an optional request-id filter) that a future multi-session server
loop can call it once per inbound session rather than reinventing
reassembly, but this phase doesn't build that loop itself.

## Decisions

**GIOP header fields never split across fragments — this is what makes
reassembly tractable.** Per GIOP 1.2, if a message is fragmented, the
first datagram still carries every fixed header field (request_id,
target, operation, etc. for Request; service_context, request_id,
reply_status for Reply) complete; only the trailing opaque argument/
result bytes may be split. This means Phase 1's
`hace_giop_decode_request_header`/`decode_reply_header` work unmodified
on the first datagram of a fragmented message, returning the fixed
fields plus whatever initial slice of the opaque body fit. Reassembly
in `hace_uiop_receive` is therefore only ever about concatenating
opaque body bytes — never about re-parsing a header field that arrived
in pieces.

**Byte-cut fragmentation on send, with a minimum datagram size floor.**
`hace_uiop_send_message` splits at a fixed byte offset
(`max_datagram_size - 16`) rather than any GIOP-type-aware boundary.
That's only correct if the cut falls after the complete fixed header,
so `HACE_UIOP_MIN_DATAGRAM_SIZE` (256 bytes) is enforced as a floor on
`max_datagram_size` — comfortably larger than the largest fixed header
this profile defines (Request's, bounded by `HACE_GIOP_MAX_OBJECT_KEY`/
`HACE_GIOP_MAX_OPERATION_NAME` at ~134 bytes worst case). This avoids
teaching the transport layer per-message-type field boundaries.
`HACE_UIOP_MAX_DATAGRAM_SIZE` (1472, the usual MTU-safe ceiling) is the
compile-time size of this module's fixed internal buffers — no VLAs
(MISRA C:2012 Rule 18.8) — with `max_datagram_size` a runtime value
bounded between the two.

**Fragmentation is bounded on both ends.** `HACE_UIOP_MAX_FRAGMENTS`
(16) caps how many datagrams one logical message may become: send
rejects outright with `HACE_UIOP_ERR_TOO_LARGE` rather than emitting
unboundedly many datagrams, and receive abandons reassembly with the
same error rather than accumulating unboundedly from a misbehaving or
hostile peer.

**The retry-sent buffer is restored after fragmenting.**
`hace_uiop_send_message` temporarily rewrites `message`'s header (and
rewinds/advances its cursor) while splitting it into fragments, then
restores the buffer's position to its original, fully-encoded state
before returning — so `hace_uiop_call`'s retry loop can hand the same
`request` buffer to `hace_uiop_send_message` again unchanged on a
later attempt.

**Transport is injected via two explicit function pointers**
(`hace_uiop_transport_t.send`/`.recv`), not called as a hardcoded HAL
dependency. `hace_uiop_transport_for_socket` builds the production
instance (thin adapters forwarding to `hace_hal_socket_send`/`recv`).
This mirrors the HAL's own compile-time backend selection in spirit
(swap the seam, don't build a runtime dispatch registry) while being
the one point that genuinely needs a *runtime* swap: tests inject a
fault-injecting fake (`runtime/tests/test_uiop.c`'s
`fault_injection_ctx_t`, which can drop or count sends) to exercise
retry/timeout deterministically instead of depending on real network
flakiness. Two plain function pointers passed explicitly, not a vtable
— easy to trace, nothing hidden.

**`hace_uiop_receive` reports the sender's address.** A server has no
persistent connection to answer a request on — UDP gives it nothing but
the datagram's source address/port, captured from the first chunk. This
wasn't in the initial design pass and was caught while writing the
round-trip test (a server task with no way to address its reply is
useless); `out_src_addr_ipv4`/`out_src_port` parameters were added
before this phase shipped, not left as a follow-up.

**`hace_cdr_encode_raw_bytes`/`decode_raw_bytes` (added to
`runtime/include/hace/cdr.h`)**: bounds-checked `memcpy` plus cursor
advance, no length prefix, no alignment — the same convention as
`sequence<octet>`'s payload, minus the framing. Needed because a
Fragment's continuation bytes are appended raw, not re-wrapped as a
`sequence<octet>`, and nothing in Phase 1 copied already-framed bytes
verbatim like this.

**`runtime/include/hace/hal.h` moved from `runtime/src/hal/` into the
public include tree.** It was previously private (reachable only via
`runtime/`'s `PRIVATE src/` include path), which was fine while only
`.c` files under `src/` needed it. `uiop.h` — a public header, alongside
`cdr.h`/`giop.h` — needs `hace_hal_status_t`/`hace_hal_socket_handle_t`
for its transport function-pointer types, and `hal.h` itself was always
platform-neutral (the interface, not a backend implementation), so
moving it was the correct fix rather than a workaround. The four
`#include` sites that referenced its old path (`hal_pool.c`,
`hal_linux.c`, `hal_integrity178.c`, `test_hal.c`) were updated to match.

## Consequences

- An attempt that times out mid-reassembly (first chunk received, then
  the deadline elapses before all Fragments arrive) discards that
  partial state and the next attempt resends the whole request from
  scratch. A server that was mid-reply when this happens may end up
  processing the same logical request twice. This is an inherent
  property of retry-based RPC over an unreliable transport (real
  GIOP/IIOP retry semantics have a milder version of the same issue);
  solving it requires server-side request idempotency/dedup, which
  needs a server to exist first (Phase 3), so it's not addressed here.
- Phase 3's POA/dispatch loop can reuse `hace_uiop_receive` directly for
  its own inbound handling (passing `HACE_GIOP_MSG_REQUEST` and
  `match_request_id = NULL`) rather than building reassembly a second
  time, though it will need its own *multi*-session bookkeeping on top
  (this module only tracks one reassembly session per call, matching
  its single-outstanding-call scope).
