# INTEGRITY-178 tuMP HAL backend — blocked on Green Hills SDK access

This backend implements `runtime/src/hal/hal.h` for Green Hills
INTEGRITY-178 tuMP. It currently compiles as a **stub**: every function
returns `HACE_HAL_ERR_NOT_IMPLEMENTED`.

## Why it's a stub

Green Hills' INTEGRITY-178 SDK, headers, and API documentation are
licensed and access-controlled (typically under NDA). This project was
built without access to them, so the real implementation — INTEGRITY
sockets/networking calls, `Time_GetTime`/equivalent, INTEGRITY task
creation and priority APIs — could not be written or tested here.

## What's needed to complete this backend

Whoever has Green Hills SDK access needs to fill in
`hal_integrity178.c` (to be added) implementing:

- `hace_hal_time_now` — using INTEGRITY-178's monotonic time facility.
- `hace_hal_socket_open/close/send/recv` — using INTEGRITY-178's
  networking stack (BSD-socket-like API, if the target's network add-on
  provides one) or the platform's UDP primitives, respecting tuMP
  partitioning (a socket belongs to a partition/address space; this HAL
  assumes one address space per ORB instance, matching how `hal_linux.c`
  behaves).
- `hace_hal_task_create/join` — mapped onto INTEGRITY-178 tasks within
  the current partition, with `priority` mapped onto INTEGRITY's
  partition-scoped priority range.

The function contracts (blocking behavior, bounded-datagram-size
rejection instead of truncation, no dynamic allocation) are specified in
`runtime/src/hal/hal.h` and must be preserved exactly — the runtime above
the HAL is written once against that contract and must behave the same
on both targets.

## Testing

Once implemented, this backend should be exercised against the same
`runtime/tests/` suite as the Linux backend, cross-compiled with Green
Hills' MULTI toolchain and run on target hardware or the GHS simulator —
neither of which is available in this environment. `cmake/integrity178.cmake`
is a placeholder toolchain file; it will need real GHS compiler/linker
paths and flags filled in alongside this backend.
