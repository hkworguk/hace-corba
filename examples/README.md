# hace-corba examples

Four real client/server pairs, each two separate executables talking
CORBA over UDP via the actual ORB (`hace_corba_poa_t`, `hace_uiop_call`/
`send_message`/`receive`, `hace_corba_ior_t`). Each pair is implemented
three times — once per supported language binding — against the exact
same three `.idl` files (reference only, not processed by any tool:
there's no `idlc` yet, Phase 4, to generate the servants/stubs each
implementation hand-writes instead).

## Layout

Grouped by example type first, language second:

```
examples/<name>/<name>.idl   The interface(s) -- what operations exist.
examples/<name>/server.idl   What the server implements (#includes <name>.idl).
examples/<name>/client.idl   What the client calls (#includes <name>.idl).
examples/<name>/c/           C implementation (links the certified runtime directly).
examples/<name>/rust/        Rust implementation (bindings/rust/hace-corba).
examples/<name>/ada95/       Ada95 implementation (bindings/ada95/hace_corba).
examples/common/<lang>/      Shared per-language example helpers (IOR parsing,
                             announcing a server's address) -- one copy per
                             language, used by every pair in that language.
```

Three IDL files per pair, not one: `<name>.idl` states the
interface(s) themselves; `server.idl` and `client.idl` each `#include`
it and state which side implements what and which side calls what —
for the three one-directional pairs that's a plain split (the server
implements, the client calls), but `observer_attach` is bidirectional,
so its `server.idl` *and* `client.idl` each both implement one
interface and call the other (see its own files for the details).
Every server-side source file across all three languages points to
its pair's `server.idl`; every client-side one points to `client.idl`.

| Example | Demonstrates |
|---|---|
| `rpc_basic` | A synchronous twoway call. |
| `oneway` | Fire-and-forget calls (no wait, no reply). |
| `async_ami` | Poller-style async invocation: send, do other work, poll for the reply until it lands. |
| `observer_attach` | Bidirectional calls: the observer runs its own POA to receive callbacks after attaching to the subject. |

`examples/cdr_udp_echo/` is older and simpler (CDR + HAL only, no
ORB/POA/GIOP) — it predates the four pairs above and has no `.idl`
(there's no CORBA interface to state).

## Building and running

Each pair/language combination has its own `build.sh` and `run.sh`
(e.g. `examples/rpc_basic/rust/build.sh`): `build.sh` builds that
language's runtime dependency first if it isn't already built, then
the pair's two executables; `run.sh` starts the server in the
background, runs the client against it, and cleans up the server
either way.

```sh
examples/rpc_basic/rust/build.sh
examples/rpc_basic/rust/run.sh
```

All four pairs' servers are self-terminating (handle a fixed number of
calls, or time out) — no Ctrl-C needed for a normal run. `ci/build_all.sh`
runs every pair in every language as part of the full build.

## Design notes

- **IOR discovery**: `hace_corba_object_to_string`/`string_to_object`
  (Phase 3) round-trip a server's address as `uiop:host:port/hexkey`.
  Each language's `examples/common/<lang>/` has the shared parse-
  argument-or-default-to-localhost helper every client uses.
- **`observer_attach`** is the one with real structure: both programs
  run a POA. The observer calls `Subject.attach(self_ior)` as a
  twoway client call, then dispatches incoming `Observer.update()`
  oneway calls on that same local POA. The subject generates events
  on its own schedule and pushes them to every attached observer as a
  oneway call — no BiDirIIOP-style negotiation needed (see
  `docs/architecture/adr-0002-giop-framing.md`/
  `adr-0003-uiop-transport.md`), just two ordinary object references.
- **One IDL spec, three implementations**: `examples/<name>/{<name>,server,client}.idl`
  are reference documentation shared by all three languages, not
  machine-generated input -- each language's servant/stub is hand-
  written against its own binding, and the three are expected to stay
  behaviorally identical (same ports, object keys, operation names,
  and test values across all three).
