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

The tree now contains Windows-friendly build targets and has been validated with
Visual Studio 2022/MSVC; follow the Windows quick start below when building on
that platform.

## Requirements

- CMake ≥ 3.20
- A C++20-capable compiler (tested with GCC 13, Clang 16, MSVC 19.36+)
- Python 3 (already required by the WABT build scripts)
- On Windows install the Visual Studio 2022 C++ workload (or Build Tools) and run
  commands from an *x64 Native Tools* developer prompt so MSVC and CMake share
  the same environment.

Optional: pass `-DWAT2WASM_EXECUTABLE=/path/to/wat2wasm` if you prefer an
external assembler, or disable the bundled copy with
`-DWASM_INTERP_USE_BUNDLED_WABT=OFF`.

## Building

```bash
cmake -S . -B build
cmake --build build
```
Multi-config generators (Visual Studio, Xcode)
require the usual `--config Debug|Release` flag on the second command.

Note: The configure step detects the lean WABT checkout under `wabt/` and wires
`wat2wasm` into the build graph. 

Examples are built by default; disable them with
`-DWASM_INTERP_BUILD_EXAMPLES=OFF` if you only need the library/tests.

### Windows (MSVC) Quick Start

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target run_wat_module wasm_interp_tests
ctest --test-dir build -C Release
```

The bundled `wat2wasm.exe` ends up under `build/wabt/Release/` (swap in your
chosen configuration) and the example runner lives at
`build/Release/run_wat_module.exe`. Tests can also be driven from the Visual
Studio Test Explorer; both approaches exercise the same CTest entries.


## Testing

Use the staged harness for exhaustive coverage:

```bash
cmake --build build --target wasm_interp_tests
ctest --test-dir build                     # or add -C Debug|Release for MSVC
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

2. Build the example runner via CMake (already part of the default build when
   `WASM_INTERP_BUILD_EXAMPLES=ON`):
   ```bash
   cmake --build build --target run_wat_module
   # Visual Studio generators: add --config Release (or Debug)
   ```
   Note: this runner is a simple example and will surely not be enough for bigger projects. It serves as a template to facilitate embedding of this interpreter (more detail below).

3. Execute the runner:
   ```bash
   ./build/run_wat_module path/to/module.wasm            # single-config generators
   .\build\Release\run_wat_module.exe path\to\module.wasm # Visual Studio / MSVC (adjust Debug|Release)
   ```

In `projects/` you can find some `.wat` files (and their `.c` equivalent if any) which this interpreter supports. If from C files, the `.wat` files have been generated with `wasm2wat` tool from `wabt` after being compiled to `.wasm` with emscripten.

## Embedding the Interpreter

### 1. Link the library

`wasm_interp` is a static CMake target. Pull the project into your workspace (as
a subdirectory, submodule, or via FetchContent) and link it to your app:

```cmake
add_subdirectory(external/WasmInterpCpp)    # adjust path as needed
target_link_libraries(my_app PRIVATE wasm_interp)
```

The public headers live under `include/`, so `#include "wasm/interpreter.hpp"`
is all you need in client code.

### 2. Load and run a module

```cpp
#include "wasm/interpreter.hpp"

int main()
{
    const auto bytes = wasm::read_file("module.wasm");

    wasm::Interpreter interp;
    interp.load(bytes);              // parse + instantiate

    auto result = interp.invoke("_start");
    if (result.trapped) {
        std::cerr << "trap: " << result.trap_message << "\n";
        return 1;
    }

    for (const auto& value : result.values) {
        // consume returned values; Value exposes helpers like as<int32_t>()
    }
}
```

`Interpreter::invoke` accepts arbitrary export names and parameters; pass values
in `std::span<const Value>` order matching the function signature. Reuse the
same interpreter instance to call multiple exports—the module stays loaded until
the object is destroyed.

### 3. Provide host imports

Register imports before calling `load`. The interpreter ships with
`wasi_snapshot_preview1` shims for common WASI syscalls, and you can expose your
own host bindings with the `register_host_*` helpers:

```cpp
wasm::Interpreter interp;

interp.register_host_function(
    "env",                 // module name expected by the Wasm file
    "print_i32",           // import name
    {{wasm::ValueType::I32}, {}},  // params, results
    [](std::span<const wasm::Value> args) -> wasm::ExecutionResult {
        std::cout << args[0].as<int32_t>() << "\n";
        return {};
    });

interp.register_host_memory("env", "memory", {/* MemoryType */}, initial_bytes);
interp.register_host_global("env", "tick", {/* GlobalType */}, wasm::Value::make<int32_t>(0));

interp.load(bytes);
```

Use the matching `register_host_memory`, `register_host_table`, and
`register_host_global` calls when your module imports those resources. The
helper functions perform validation so mismatched limits or element types fail
fast during instantiation.

### 4. Inspect memory (optional)

Call `Interpreter::memory()` to get a mutable view of the first exported memory:

```cpp
auto mem = interp.memory();
if (mem.data) {
    std::span<uint8_t> bytes(mem.data, mem.size);
    // read / write linear memory
}
```

This is useful for exchanging bulk data with the guest. Grow memory inside Wasm
using the `memory.grow` instruction or from the host through exported functions.

### 5. Error handling & diagnostics

- `ExecutionResult::trapped` is set when the guest triggered a trap (for
  example via `unreachable`, out-of-bounds memory, or an imported function
  returning a trapped state). The interpreter propagates a descriptive message.
- Structural errors (invalid module, missing imports, type mismatches) throw
  `std::runtime_error` from `load`—wrap instantiation in a try/catch if you need
  to surface these gracefully.

With these pieces you can embed the interpreter into CLI tools, servers, or unit
tests without touching the rest of the repository.

## Repository Layout

- `src/` – interpreter and module loader implementation.
- `include/` – public API headers and POD module definitions.
- `tests/` – staged `.wat` fixtures and C++ harness.
- `examples/` – minimal runner showing how to execute a compiled module.
- `projects/` – simple `.wat` files successfully assembled/ran with the interpreter. Also contains C code if `.wat` is generated from `.c` (through emscripten)
- `wabt/` – trimmed WABT subset providing `wat2wasm`.

For deeper implementation details, consult `docs/IMPLEMENTATION.md`.
