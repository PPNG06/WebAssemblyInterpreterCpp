# WasmInterpCpp Implementation Notes

This document captures the current design, scope, and known limitations of the
`WasmInterpCpp` interpreter. It is meant to be a living record as additional
features are added.

## Architecture Overview

- **Front-end (binary loader)** – `wasm::parse_module` in
  `src/module_loader.cpp` decodes the WebAssembly 1.0 (MVP) binary format into
  the plain-data structures defined under `include/wasm/module.hpp`. The parser
  enforces section payload bounds, records constant expressions for globals,
  data segments, and element offsets, and retains raw code bodies for the
  executor.
- **Runtime / executor** – `wasm::Interpreter` (backed by
  `Interpreter::Impl` in `src/interpreter.cpp`) instantiates a module, applies
  memory and table initialisers, and executes bytecode with a classic operand
  stack machine. Control-flow is modelled with a stack of `ControlFrame`s that
  tracks block signatures and jump destinations.
- **Host interface** – Imports resolve against registries keyed by
  `(module, name)`. Functions are backed by host callbacks; linear memories,
  tables, and globals can also be supplied from the embedder so modules that
  import them instantiate successfully. The builtin WASI shim currently wires
  up `wasi_snapshot_preview1.fd_write` and `wasi_snapshot_preview1.proc_exit`,
  providing minimal stdout/stderr support while leaving the rest of the surface
  to the embedding application.
- **Testing harness** – `tests/cpp/test_main.cpp` assembles the staged `.wat`
  fixtures to `.wasm` during the build (via the `generate_wasm` target) and runs
  each exported scenario, checking observable
  state via linear memory. Extra samples live under `tests/custom_tests/` for
  integration coverage such as imported memories/tables/globals.

## Instantiation & Execution Flow

1. `Interpreter::load` forwards the supplied bytes to `parse_module`, producing
   a `Module` struct that mirrors the binary sections (types, imports, codes,
   data/element segments, exports, optional `start`, …).
2. `Interpreter::Impl::instantiate` resets the runtime state, resolves imports
   through the host registries, materialises local functions/globals/memories/
   tables, then caches passive data segments so bulk-memory instructions can
   reference them later. Element segments are applied eagerly to populate
   funcref tables.
3. If the module declares a `start` function, it is executed immediately via
   `execute_function`.
4. Every subsequent `invoke` pushes a function frame onto a control stack,
   allocates locals, and interprets the body with a bytecode reader. Structured
   control flow is implemented with a vector of `ControlFrame`s; branches call
   `branch(...)` to pop values according to the target block signature,
   adjusting the operand stack, and—if jumping to the function frame—use the
   cached signature captured up front so the interpreter never reads past the
   control stack.
5. Operand values carry a lightweight `ValueOrigin` tag. This lets store-like
   instructions disambiguate which stack entry to consume when multi-value
   calls or loads leave additional results on the stack. The tag is also passed
   through load operations so hosts can tell when values originate from memory.

## Implemented Feature Set

The interpreter deliberately focuses on the WebAssembly MVP in order to satisfy
the progression of `tests/wat/*`. Supported surface area includes:

- Function definitions, parameter and result types, locals, exports, and the
  `start` function.
- Mutable and immutable globals with constant-expression initialisers.
- Linear memory (creation, reads/writes, `memory.size`, `memory.grow`), data
  segments, and bounds checking for all memory accesses.
- Tables of funcrefs, element segments, and `call_indirect` with signature
  checks.
- Full coverage of the numeric instruction set used by the test suite,
  including integer/float arithmetic, comparisons, bitwise ops, conversions,
  reinterpret casts, and NaN-aware min/max/nearest behaviour.
- Structured control flow (`block`, `loop`, `if`, `else`, `br`,
  `br_if`, `br_table`, `return`, `unreachable`) with the proper operand stack
  shuffling mandated by the spec.
- Function imports via a host registry. Two WASI Preview 1 shims are provided
  by default:
  - `wasi_snapshot_preview1.fd_write` prints to `stdout`/`stderr`, writes the
    byte count to the supplied pointer, and returns a WASI errno (0 on success).
  - `wasi_snapshot_preview1.proc_exit` reports termination as a trap so callers
    can observe the exit status.
- Convenience APIs (`register_host_function`, `register_host_memory`,
  `register_host_table`, `register_host_global`) let embedders preload the
  resources that modules import.
- Post-MVP execution covering the subset exercised by
  `tests/wat/08_test_post_mvp.wat`: multi-value returns, reference types
  (`ref.null`, `ref.func`, `ref.is_null`), externref/funcref tables (including
  `table.get`, `table.set`, `table.size`, `table.grow`, `table.fill`,
  `table.copy`), and their interaction with bulk memory operators.
  
Every feature above is covered indirectly by the staged test suite:
- `tests/wat/01_test.wat` exercises integer arithmetic, bitwise operations,
  locals, globals, load/store variants, and branching.
- `tests/wat/02_test_prio1.wat` layers in function calls, recursion, floating
  point arithmetic, conversions, select/drop/nop, and memory growth.
- `tests/wat/03_test_prio2.wat` adds tables, `call_indirect`, 64-bit integer
  paths, and element/data segment usage.
- `tests/wat/04_test_prio3.wat` stress-tests edge cases (unsigned comparisons,
  rotations, NaN-sensitive float ops, and combined memory scenarios).
- `tests/wat/05_test_complex.wat` validates deep/nested control flow, branching
  with results, loop label management, and multi-call interactions.
- `tests/wat/06_test_fc.wat` covers the non-trapping float-to-int conversions
  (0xFC prefix), ensuring saturating semantics for all f32/f64 to i32/i64
  variants and a suite of edge cases (NaNs, infinities, extremes).
- `tests/wat/07_test_bulk_memory.wat` exercises the bulk memory proposal
  instructions (`memory.copy`, `memory.fill`, `memory.init`, `data.drop`) across
  overlapping regions, zero-length operations, and passive data reuse.
- `tests/wat/08_test_post_mvp.wat`: multi-value returns, reference types
  (`ref.null`, `ref.func`, `ref.is_null`), externref/funcref tables (including
  `table.get`, `table.set`, `table.size`, `table.grow`, `table.fill`,
  `table.copy`), and their interaction with bulk memory operators.

Running `ctest` in the build directory relies on the generated fixtures from the
previous build step (the default build already triggers `generate_wasm`), then
executes all exported entry points and validates memory side effects.

## Design Decisions & Rationale

- **Stack-machine execution** – Staying close to the specification’s abstract
  machine makes correctness reasoning and future extensions (e.g. new opcodes)
  straightforward. Operand stack operations are type-checked; failures signal
  traps that propagate back to the caller.
- **Host callback registry** – Imports are resolved against `(module, name)`
  keys. Built-in shims keep an internal context pointer, while the public API
  lets embedders supply a simple `std::function` receiving a span of `Value`s.
  This keeps the execution core unaware of embedding specifics yet makes common
  integrations straightforward.
- **Strict bounds enforcement** – All memory, table, and stack accesses are
  guarded. Violations raise `Trap`s or `std::runtime_error`s, which helps catch
  malformed modules early and mimics the behaviour of production runtimes.
- **Default WASI coverage** – Minimal `fd_write`/`proc_exit` support is enough
  to execute common command-line WASM programs without pulling in a full WASI
  layer. The design leaves room to add more functions over time.
- **Post-MVP opcode support** – Beyond the 0xFC-prefixed saturating
  float-to-int conversions, the executor implements the reference-types and
  bulk-memory proposals used by the test suite. Operand stack entries retain a
  lightweight `ValueOrigin` tag so stores can preserve operand order even when
  multi-value calls or table loads leave additional values on the stack.
- **Bulk memory operations** – Passive data segments are preserved at
  instantiation time so the 0xFC bulk-memory opcodes can safely copy, fill, and
  drop data with proper bounds checking and memmove semantics for overlapping
  regions.

## Known Limitations & Future Work

- The builtin WASI surface is intentionally tiny: only `fd_write` and
  `proc_exit` are registered out of the box. Modules that rely on args/env,
  clocks, randomness, or filesystem calls must be paired with host callbacks
  supplied by the embedder. `proc_exit` surfaces as a trap so hosts can decide
  how to propagate exit codes.
- SIMD, GC/typed function references, tail calls, and memory64 remain
  unsupported. These features are outside the current test-driven scope and will
  trap if encountered.
- Declarative element segments and their bulk table operators are still on the
  backlog: `table.init` and `elem.drop` intentionally trap until that support
  lands.
- `Interpreter::memory()` exposes only the first linear memory. Additional
  memories can still be imported or defined, but hosts need explicit exported
  helpers to access them.

These constraints are documented so subsequent increments can tackle them with
clear expectations.

## Build & Test

```bash
cmake -S . -B build
cmake --build build

#(optional) to enable tests:
cmake --build build --target wasm_interp_tests
ctest --test-dir build
```

The top-level build now produces the vendored `wat2wasm` tool automatically; no
separate WABT configuration step is required. If you already have an external
assembler, pass `-DWAT2WASM_EXECUTABLE=/path/to/wat2wasm` or disable the bundled
copy with `-DWASM_INTERP_USE_BUNDLED_WABT=OFF`.

On Windows/Visual Studio, configure with your preferred generator and architecture,
then pass `--config Debug|Release` to the build and test invocations:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --target generate_wasm
ctest --test-dir build --config Debug
```

The generated `wat2wasm.exe` lives under `build/<Config>/wabt/`.

Only the subset of WABT sources required to build `wat2wasm` is kept under
`wabt/`; auxiliary tooling, documentation, and git metadata were removed so the
vendored tree stays lightweight while preserving licensing.

The build uses only standard C++20 and has been vetted with `-Wall -Wextra -Wpedantic`,
keeping portability to Windows/MSVC in mind.

### Regenerating Test Fixtures

All `.wat` programs under `tests/wat/` are assembled to `.wasm` binaries inside
`build/generated_wasm/`. A custom CMake target wires this together:

```bash
cmake --build build --target generate_wasm          # single-config generators
cmake --build build --target generate_wasm --config Debug  # multi-config example
```

Invoking `generate_wasm` will first build the bundled `wat2wasm` executable (if
needed) and then re-assemble only the `.wat` files that changed since the last
run. This is the quickest way to refresh the fixtures before running tests.

You can also build just the assembler itself:

```bash
cmake --build build --target wat2wasm [--config <cfg>]
```

The tool is emitted inside the build tree (`build/wabt/wat2wasm` on Unix-like
single-config generators); use the extra `--config` argument when working with
Visual Studio, Xcode, or other multi-config toolchains.

### Targeted Test Runs

The executable `wasm_interp_tests` understands a few helper flags so you can
inspect or drill into individual scenarios without running the entire suite:

```bash
# enumerate every registered (module.case) identifier
./build/wasm_interp_tests --list

# execute every case in a specific module
./build/wasm_interp_tests 05_test_complex

# run a single case by module + export name
./build/wasm_interp_tests 05_test_complex multi_call
```

CTest mirrors that granularity—each module is exposed as `module.<name>` and
each export as `<module>.<case>`. You can list them with `ctest -N`, then
filter as needed:

```bash
ctest --test-dir build -R module.05_test_complex     # module-wide
ctest --test-dir build -R 05_test_complex.multi_call # individual case
ctest --test-dir build -L case                       # only per-case entries
```

Verbose runs (`ctest -VV`) forward the harness output, making it easy to inspect
memory checks and PASS/FAIL lines for a single scenario.

### Running a Standalone `.wat` Module

The repository ships with a convenience runner (`run_wat_module`) so you can
execute arbitrary binaries without writing additional glue code.

1. **Build the example**  
   The example target is enabled by default (`-DWASM_INTERP_BUILD_EXAMPLES=ON`).
   After configuring the project, build the helper:
   ```bash
   cmake --build build --target run_wat_module
   ```
   On multi-config generators (Visual Studio, Xcode) append
   `--config Debug`/`Release` as needed. The executable is written to
   `build/run_wat_module` for single-config generators or
   `build/<Config>/run_wat_module` otherwise.

2. **Assemble your `.wat` file**  
   If the module already lives under `tests/wat/`, the `generate_wasm` target
   will assemble it to `build/generated_wasm/<name>.wasm`:
   ```bash
   cmake --build build --target generate_wasm            # add --config <cfg> as required
   ```
   For ad-hoc files, build the bundled assembler once and invoke it directly:
   ```bash
   cmake --build build --target wat2wasm
   build/wabt/wat2wasm path/to/custom.wat -o build/generated_wasm/custom.wasm
   ```
   Replace `build/wabt/wat2wasm` with `build\<cfg>\wabt\wat2wasm.exe` on Windows
   multi-config builds.

3. **Run the module**  
   Pass the assembled binary to the runner. By default it auto-detects an entry
   point (preferring `_start` for WASI-style modules) and wires in the built-in
   WASI shims for `fd_write`/`proc_exit`.
   ```bash
   ./build/run_wat_module build/generated_wasm/custom.wasm
   ```
   Use `--list-exports`, `--invoke <name>`, and the `--arg-*` flags to call
   specific exports with typed arguments. See `run_wat_module --help` for the
   full CLI surface.
