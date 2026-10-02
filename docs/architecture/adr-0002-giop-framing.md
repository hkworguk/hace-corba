# ADR-0002: GIOP message framing — version, pattern, and profile

## Status

Accepted.

## Context

Phase 1 (`docs/roadmap.md`) needed a pure, transport-independent
encode/decode layer for GIOP messages (`runtime/include/hace/giop.h`,
`runtime/src/giop/giop.c`) for Phase 2's UIOP transport to build on. GIOP
(formal/12-11-14 Sec 7) has three wire-format revisions and several
pieces of machinery (dynamic addressing, service contexts, multiple
synchronization scopes) that exist to serve TCP/IIOP's connection model
or interop with features this project doesn't implement. Building all of
it would multiply implementation and test surface without serving this
project's actual goals.

## Decisions

**GIOP 1.2 only — no 1.0/1.1.** This is a from-scratch ORB with no
legacy-interop requirement. GIOP 1.2 is the most capable revision
(unified `TargetAddress`, a clean Fragment definition) and picking one
avoids three parallel wire-format variants.

**Encode via a reusable two-pass "begin/finish" pattern** instead of
duplicating header/backfill logic per message type:
- `hace_giop_begin_message` sets the buffer's byte order and advances
  to position 16 (12-byte header + the 4 padding bytes GIOP 1.2
  requires so the body is 8-byte aligned from the start of the
  message — 16 is itself a multiple of 8, so a fresh
  `hace_cdr_buffer_t` whose position 0 *is* wire offset 16 aligns
  correctly with zero special-casing in the existing CDR alignment
  logic).
- The caller encodes the type-specific fixed fields and any opaque
  argument/result bytes into the same buffer.
- `hace_giop_finish_message` computes the body length from the current
  position, rewinds to 0, writes the real header + pad now that the
  size is known, and restores the position to the true end.

**Decode mirrors this**, solving GIOP's usual byte-order bootstrap
problem with the existing buffer machinery rather than a special case:
`hace_giop_decode_message_header` reads the flags octet (a single,
order-independent byte) and calls the new `hace_cdr_buffer_set_byte_order`
before decoding the multi-byte `message_size` field.

## Profile within GIOP 1.2 (each a documented simplification, not an
accidental omission)

- **`TargetAddress`: object key only** (discriminant 0). The
  profile-tag and IOR-addressing discriminants are rejected on decode
  (`HACE_GIOP_ERR_UNSUPPORTED_ADDRESSING_DISPOSITION`). Nothing in this
  project produces them, and Phase 3's object references are plain
  object keys.
- **Sync scope collapsed to oneway vs. "wait for the real reply."**
  `response_flags` only accepts `0` (no reply) or `3`/`WITH_TARGET`
  (reply expected); `1`/`WITH_TRANSPORT` and `2`/`WITH_SERVER` are
  rejected (`HACE_GIOP_ERR_UNSUPPORTED_SYNC_SCOPE`) — see
  `docs/corba-spec-gap-analysis.md` Sec 7 for why they have no clean
  UDP equivalent.
- **Service contexts: empty only**, in and out
  (`HACE_GIOP_ERR_NONEMPTY_SERVICE_CONTEXT` on decode otherwise). This
  project never sends one and isn't an interop target for a peer that
  would.
- **Fragment: header only.** `hace_giop_fragment_header_t`/its
  encode/decode cover just `request_id`; splitting an oversized message
  across datagrams and reassembling it is Phase 2's job, not a
  wire-format concern.
- **`LocateReply` forwarding IORs not decoded.** `OBJECT_FORWARD*`
  status can be followed by an IOR, but Phase 3 hasn't defined IOR
  encoding yet, so `hace_giop_decode_locate_reply_header` stops after
  the fixed fields regardless.
- **`HACE_GIOP_MSG_CLOSE_CONNECTION` is numbered but inert.** Decoding
  one isn't an error — the numbering stays spec-correct — but nothing
  in this module emits or acts on it; it signals a TCP connection
  tearing down, which UDP has no equivalent of (`docs/corba-spec-gap-analysis.md`
  Sec 2).

**Object key and operation name are fixed-capacity**
(`HACE_GIOP_MAX_OBJECT_KEY` = 32, `HACE_GIOP_MAX_OPERATION_NAME` = 64),
matching the no-dynamic-allocation constraint used throughout
`runtime/`. Operation arguments/results are opaque byte spans
(`hace_cdr_buffer_t` views via `hace_cdr_buffer_init_view`) since there
is no per-operation argument typing until `idlc` exists (Phase 4).

## Consequences

- Phase 2 (UIOP transport) can send/receive these messages over UDP
  without touching this module's wire format.
- A peer outside this project's own two endpoints (hypothetically
  speaking another ORB's GIOP 1.2) would likely trip the sync-scope or
  service-context rejections above. That's intentional: this project is
  not an interop target, and failing closed on anything outside the
  documented profile is preferable to silently misinterpreting it.
