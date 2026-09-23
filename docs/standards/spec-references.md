# OMG Specification References

hace-corba is built against the following specifications published by the
Object Management Group (OMG) at <https://www.omg.org/corba/> and
<https://www.omg.org/spec/CORBA>. This file pins the exact formal document
IDs and versions this project targets. Fetched and verified 2026-09-23.

Do not vendor OMG PDFs into this repository — they are copyrighted OMG
documents. Reference them by ID/URL only.

| Spec | Version | OMG document ID | URL |
|---|---|---|---|
| CORBA Core / Interfaces | 3.4 | formal/21-02-02 | `omg.org/spec/CORBA/3.4/Interfaces/PDF` |
| CORBA Interoperability (GIOP, IIOP, CDR) | 3.4 | formal/12-11-14 | `omg.org/spec/CORBA/3.4/Interoperability/PDF` |
| CORBA Components | 3.4 | formal/21-02-04 | `omg.org/spec/CORBA/3.4/Components/PDF` (not used — out of scope, see below) |
| CORBA/e (embedded profiles) | 1.0 | — | `omg.org/spec/CORBAe/1.0` |
| Real-Time CORBA | 1.2 | formal/05-01-04 | `omg.org/spec/RT/1.2/PDF` |
| IDL to Ada Language Mapping | 1.3 | — | `omg.org/spec/ADA/1.3/PDF` |
| IDL to C++11 Language Mapping | latest | — | `omg.org/spec/CPP11` |

## Scope notes

- **CORBA Components (formal/21-02-04)** is not used. The component model
  (assembly, containers, CCM) has no bearing on a static, embedded,
  DAL C-oriented ORB and would add certification surface for no benefit.

- **CORBA/e** defines two profiles for resource-constrained systems:
  - **Compact Profile** — successor to the old "Minimum CORBA" spec. Drops
    the Dynamic Invocation Interface (DII), Dynamic Skeleton Interface
    (DSI), and Interface Repository (IR). This is the profile family
    hace-corba targets.
  - **Micro Profile** — for deeply-embedded targets with even tighter
    constraints. Not targeted initially; the Compact Profile subset is a
    superset of what Micro needs, so nothing here forecloses a future Micro
    build.

- **Real-Time CORBA 1.2** is not implemented wholesale. hace-corba borrows
  its *concepts* — bounded thread pools, priority propagation from client to
  server thread — for the POA's static thread model (see
  `docs/architecture/`), rather than importing the full RTCORBA interface
  set (mutexes, priority-banded connections, etc.), which assumes a
  TCP/IIOP transport this project does not use.

- **IDL to C mapping**: OMG has no currently-maintained standalone C mapping
  specification. hace-corba defines and documents its own C mapping in
  `docs/standards/idl-c-mapping.md` (added when the IDL compiler's C backend
  is built — see `docs/roadmap.md`), kept consistent with the CORBA/e
  Compact Profile conventions and cross-checked against the Ada and C++
  mappings for naming/semantic consistency.

- **IDL to Rust mapping**: no OMG mapping exists for Rust. hace-corba
  defines a non-normative house mapping in
  `docs/standards/idl-rust-mapping.md` (added alongside the Rust backend),
  modeled on CORBA semantics: interfaces map to traits plus a generated
  stub (client) and skeleton (server) pair, exceptions map to `Result`,
  bounded sequences map to fixed-capacity `Vec`-like wrappers, oneway
  operations map to fire-and-forget functions returning immediately.

## Transport deviation

hace-corba's transport, "UIOP" (UDP Inter-ORB Protocol), reuses GIOP message
framing and CDR encoding from formal/12-11-14 but runs it over UDP instead
of TCP. This is a deliberate deviation from the standard IIOP transport
binding, not an alternative encoding of GIOP itself. See
`docs/architecture/adr-0002-uiop-transport.md` (added in the transport
phase — see `docs/roadmap.md`) and `docs/standards/uiop-profile.md` for the
full rationale and the resulting semantic differences (notably: oneway
calls are genuinely best-effort, unlike over a reliable IIOP transport).
