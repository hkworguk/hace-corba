# hace-corba C examples

Four real client/server pairs, each two separate executables talking
CORBA over UDP via the actual ORB (`hace_corba_poa_t`, `hace_uiop_call`/
`send_message`/`receive`, `hace_corba_ior_t`) — not just CDR-over-a-
raw-socket like `examples/rust/`/`examples/ada95/`. Hand-written
servants and stubs throughout: there's no `idlc` yet (Phase 4) to
generate them. Each has a `.idl` file stating the interface it
hand-implements — reference only, not processed by any tool.

| Example | Demonstrates |
|---|---|
| `rpc_basic` | A synchronous twoway call (`hace_uiop_call`). |
| `oneway` | Fire-and-forget calls (`hace_uiop_send_message`, no wait, no reply). |
| `async_ami` | Poller-style async invocation: send, do other work, poll `hace_uiop_receive` until the reply lands. |
| `observer_attach` | Bidirectional calls: the client runs its own POA to receive callbacks after attaching to the server. |

## Running

Each pair is two executables; start the server first (it prints its
own address), then the client (which defaults to that same address on
localhost, or takes it as `argv[1]` to run across machines):

```sh
./build/examples/c/rpc_basic_server &
./build/examples/c/rpc_basic_client
```

`oneway`, `async_ami`, and `observer_attach_subject_server` /
`observer_attach_observer_client` follow the same pattern. All four
servers are self-terminating (handle a fixed number of calls, or time
out) — no Ctrl-C needed for a normal run. `ci/build_all.sh` runs all
four pairs as part of the full build.

## Design notes

- **IOR discovery**: `hace_corba_object_to_string`/`string_to_object`
  (Phase 3) round-trip a server's address as `uiop:host:port/hexkey`.
  `examples/c/common/example_common.h` has the shared parse-argument-
  or-default-to-localhost helper every client uses.
- **`observer_attach`** is the one with real structure: both programs
  run a POA. `observer_client` calls `Subject.attach(self_ior)` as a
  twoway client call, then dispatches incoming `Observer.update()`
  oneway calls on that same local POA. `subject_server` generates
  events on its own schedule and pushes them to every attached
  observer via `hace_uiop_send_message` — no BiDirIIOP-style
  negotiation needed (see `docs/architecture/adr-0002-giop-framing.md`/
  `adr-0003-uiop-transport.md`), just two ordinary object references.
