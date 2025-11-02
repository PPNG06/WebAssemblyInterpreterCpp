# Missing Features & Known Gaps

The items below enumerate the observable limitations of the current interpreter/runner, along with follow-up work needed.

## 1. Incomplete WASI Preview 1 surface
- The runner now hand-authors lightweight shims for `fd_read`, `fd_close`, `fd_seek`, `fd_fdstat_get`, `fd_prestat_*`, and `path_open` in addition to the existing `fd_write`, `proc_exit`, `args_*`, `environ_*`, `clock_time_get`, and `random_get`. These helpers are intentionally minimal: files are opened read-only off the host working directory, only a single preopened directory (`.`) is exposed, and no write/delete operations are supported.
- Remaining gaps include directory traversal (`path_readlink`, `fd_readdir`, etc.), file metadata mutation (`fd_filestat_*`), socket/poll APIs, and full rights/flag handling.
- **Next steps**
  1. Flesh out the remaining syscalls (directory iteration, metadata updates, file creation/removal, sockets) or switch to a full WASI layer such as `uvwasi` once the lightweight shims hit diminishing returns.
  2. Tighten capability checks: honour incoming `rights_base`/`rights_inheriting`, validate `oflags`/`fdflags`, and enforce sandbox boundaries beyond a single preopen.
  3. Add integration tests that cover stdin reads, file reads via `path_open`, and edge cases (EOF, seek past end) to lock in the current behaviour before expanding functionality further.

## 2. Importing tables/memories/globals
- The interpreter now accepts imported tables, memories, and globals via the new `register_host_*` helpers; the test suite exercises all three in `tests/custom_tests/custom_imports.wat`. Imported memories are copied into the instance on instantiation, tables honour their declared limits, and mutable globals round-trip values back into the module.
- **Remaining gaps / next steps**
  1. Provide zero-copy sharing for imported memories/tables so hosts can observe mutations without peeking through `Interpreter::memory()` (e.g., by permitting shared backing storage or callbacks on grow operations).
  2. Allow partial compatibility rather than requiring exact limit equality—hosts should be able to offer a superset of the requested limits.
  3. Generalise the API to support multiple imported memories/tables once multi-memory/table modules are introduced.

## 3. No support for post-MVP proposals beyond bulk-memory/reference types
- The interpreter lacks threads/atomics, SIMD, tail calls, exception handling, component model, GC, and other newer proposals.
- **Next steps**
  1. Prioritise proposals based on intended workloads (e.g., add atomics if targeting multithreaded WASM).
  2. For each chosen proposal, extend the binary reader, module model, and execution engine with the new opcodes/types, accompanied by tests sourced from the proposal’s official suites.

## 4. Runner limitations (observability & tooling)
- `examples/run_wat_module.cpp` currently provides minimal debugging aid (no instruction tracing or memory dumps) which makes diagnosing interpreter bugs tedious.
- **Next steps**
  1. Add optional flags (e.g., `--trace`, `--dump-memory <range>`) gated behind command-line switches.
  2. Surface exit codes and trap details more richly (JSON/text output) for tooling integration.

Feel free to append further gaps as new workloads expose them;
