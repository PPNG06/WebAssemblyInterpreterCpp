# WasmInterpCpp

A self-contained WebAssembly (MVP + selected post-MVP features) interpreter
written in C++20. The project bundles a lean slice of WABT so `wat2wasm` can be
built alongside the interpreter—no external toolchain setup is required.

- WebAssembly binary loader with full MVP section parsing.
- Stack-based bytecode interpreter supporting multi-value returns, reference
  types, bulk memory operators, and an expanding WASI Preview 1 shim layer
  (stdio, args/env, clocks, randomness, read-only filesystem access).
- Test harness that assembles the staged `.wat` suites into `.wasm` and validates
  observable linear memory results.

This project was made on a Linux environment. While it should be supported on Windows, the instructions in this file (as well as the pathnames) might need closer examination.

## Requirements

- CMake ≥ 3.20
- A C++20-capable compiler (tested with GCC 13, Clang 16, MSVC 19.36+)
- Python 3 (already required by the WABT build scripts)

Optional: pass `-DWAT2WASM_EXECUTABLE=/path/to/wat2wasm` if you prefer an
external assembler, or disable the bundled copy with
`-DWASM_INTERP_USE_BUNDLED_WABT=OFF`.

## Building

```bash
cmake -S . -B build
cmake --build build
```

The configure step detects the lean WABT checkout under `wabt/` and wires
`wat2wasm` into the build graph. Multi-config generators (Visual Studio, Xcode)
require the usual `--config Debug|Release` flag on the second command.

### Windows (MSVC) Quick Start

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

The bundled `wat2wasm.exe` ends up under `build/wabt/Release/` (swap in your
chosen configuration). Tests can also be driven from the Visual Studio Test
Explorer; both approaches exercise the same CTest entries.


## Testing

Use the staged harness for exhaustive coverage:

```bash
cmake --build build --target wasm_interp_tests
ctest --test-dir build                     # or add -C Release for MSVC
```

Individual cases can be invoked directly:

```bash
./build/wasm_interp_tests --list                       # enumerate suite entries
./build/wasm_interp_tests 05_test_complex              # run an entire group
./build/wasm_interp_tests 05_test_complex multi_call   # single export
```

CTest mirrors the same structure, so you can run `ctest -N` to inspect target
labels.

### Regenerating Test Fixtures

All `.wat` files under `tests/wat/` are assembled into binaries inside
`build/generated_wasm/` via a convenience target:

```bash
cmake --build build --target generate_wasm
```

Only changed fixtures are reassembled. To build just the assembler:

```bash
cmake --build build --target wat2wasm
```

The executable ends up in `build/wabt/` on single-config generators (append the
configuration directory on MSVC).

## Running a `.wat` (or its `.wasm`) Module

1. Build `wat2wasm` (if not already done) and assemble your module:
   ```bash
   cmake --build build --target wat2wasm
   build/wabt/wat2wasm path/to/module.wat -o path/to/module.wasm
   ```
    (For Windows the path to wat2wasm should be `build/wabt/Release/wat2wasm.exe` (or with `Debug`))

2. Compile the example runner:
   ```bash
   g++ -std=c++20 -Iinclude examples/run_wat_module.cpp build/libwasm_interp.a -o build/run_wat_module
   ```
   On windows, link to `build/Release/wasm_interp.lib` (or `Debug`) on MSVC.
3. Execute the runner:
   ```bash
   ./build/run_wat_module path/to/module.wasm
   ```

The example invokes the exported `_start` function; modify the source
to call other symbols.

## Embedding the Interpreter

Link against the static library and drive the public API:

```cpp
#include "wasm/interpreter.hpp"

std::vector<uint8_t> bytes = wasm::read_file("module.wasm");
wasm::Interpreter interp;
interp.load(bytes);
auto result = interp.invoke("_start");
if (result.trapped) {
    // handle trap
}
```

Use the `register_host_*` family to preload host functionality before calling
`load`:
- `register_host_function` exposes callbacks, alongside the builtin
  `wasi_snapshot_preview1` shims for stdio/exit/argv/env, clocks, and random
  number generation.
- `register_host_memory`, `register_host_table`, and `register_host_global`
  supply imported resources so modules with external memories/tables/globals
  instantiate successfully.

## Repository Layout

- `src/` – interpreter and module loader implementation.
- `include/` – public API headers and POD module definitions.
- `tests/` – staged `.wat` fixtures and C++ harness.
- `examples/` – minimal runner showing how to execute a compiled module.
- `wabt/` – trimmed WABT subset providing `wat2wasm`.

For deeper implementation details, consult `docs/IMPLEMENTATION.md`.
