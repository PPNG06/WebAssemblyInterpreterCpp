# WasmInterpCpp on Windows

This guide walks through building, testing, and running the interpreter on
Windows 10/11 using Visual Studio’s MSVC toolchain. Adjust paths for your local
setup as needed.

## Prerequisites

- **Visual Studio 2022** (17.6 or newer) with the “Desktop development with C++”
  workload, providing MSVC and CMake integration.
- **CMake 3.20+** (ships with recent Visual Studio; available on PATH as
  `cmake.exe`).
- **Python 3** (required by the bundled WABT scripts; install from
  https://www.python.org/downloads/ if not already present).

Ensure `cmake`, `python`, and the MSVC compiler are visible from the `x64 Native
Tools Command Prompt for VS 2022`, or enable the “Developer Command Prompt”
option inside Visual Studio’s terminal.

## Configure & Build

From an MSVC-enabled command prompt:

```bat
cmake -S . -B build -G "Ninja Multi-Config"
cmake --build build --config Release
```

You can substitute `"Ninja Multi-Config"` with `"Visual Studio 17 2022"` if you
prefer the IDE-generated solution (`WasmInterpCpp.sln`). Use `--config Debug`
for a debug build.

The configure step pulls in the trimmed WABT sources under `wabt/` and sets up
the `wat2wasm` executable. The build command then compiles the interpreter,
bundled assembler, and staged fixtures.

## Regenerating `.wasm` Fixtures

To reassemble the text fixtures after changes:

```bat
cmake --build build --target generate_wasm --config Release
```

This target rebuilds `wat2wasm.exe` if required and converts any modified `.wat`
files into `build\generated_wasm\<name>.wasm`. Building the assembler alone:

```bat
cmake --build build --target wat2wasm --config Release
```

The resulting executable lives at `build\wabt\Release\wat2wasm.exe` (or
`build\wabt\Debug\wat2wasm.exe` for debug builds).

## Running the Test Suite

```bat
cmake --build build --target wasm_interp_tests --config Release
ctest --test-dir build --config Release
```

`ctest -N` lists all individual module/export cases. To run a single case:

```bat
ctest --test-dir build --config Release -R 05_test_complex.multi_call
```

You can also invoke the harness directly:

```bat
build\Release\wasm_interp_tests.exe 05_test_complex multi_call
```

## Running a Custom `.wat` Module

1. Assemble the module:
   ```bat
   cmake --build build --target wat2wasm --config Release
   build\wabt\Release\wat2wasm.exe path\to\module.wat -o path\to\module.wasm
   ```

2. Build the example runner, linking against the Release static library:
   ```bat
   cl /std:c++20 /EHsc /I include examples\run_wat_module.cpp ^
      build\Release\wasm_interp.lib /Fe:build\run_wat_module.exe
   ```
   (Use `Debug\wasm_interp.lib` when compiling a debug runner.)

3. Execute the runner:
   ```bat
   build\run_wat_module.exe path\to\module.wasm
   ```

The sample runner calls `br_table_nested_2`; edit `examples\run_wat_module.cpp`
if you need to invoke a different export.

## Embedding Tips

- Include headers from `include\wasm\` and link against
  `build\<config>\wasm_interp.lib`.
- Register host callbacks with `Interpreter::register_host_function` before
  calling `load`.
- WASI Preview 1 shims (`fd_write`, `proc_exit`) are pre-registered.

For deeper implementation details, see `docs/IMPLEMENTATION.md`.
