# Core85 Runbook

This runbook is the working guide for building, testing, extending, and debugging Core85 during development.

## Purpose

Use this document when you need to:

- set up a machine for local development
- build the project
- run or extend tests
- understand the current architecture
- troubleshoot common issues
- decide what to work on next

## Source of Truth

- Product and technical requirements: [docs/Core85_SRS_v1.0.docx](docs/Core85_SRS_v1.0.docx)
- Build configuration: [CMakeLists.txt](CMakeLists.txt)
- Core library: [src/core](src/core)
- GUI application: [src/gui](src/gui)
- Tests: [tests](tests)

## Current Implementation Snapshot

As of the current codebase:

- The core library is the most mature part of the project.
- `CPU` exposes reset, step, state snapshot, memory loading, port access, and cycle tracking.
- `Memory` supports a full 64 KB address space with ROM guards.
- `IOBus` supports 256 input and 256 output ports with callbacks on output writes.
- Only a minimal execution path exists right now, with `NOP` and `HLT` implemented.
- The assembler is a placeholder for the later Phase 3 milestone.
- The GUI is scaffolded but not yet wired to the emulator.

## Toolchain

Install these tools before expecting full local builds to work:

- CMake `3.25+`
- A C++17 compiler
- Qt `6.x` with `Widgets`
- A generator supported by CMake such as Ninja or Unix Makefiles

Optional but helpful:

- `clang-format`
- `clang-tidy`
- `lldb` or `gdb`

## Standard Workflows

### Configure With GUI on macOS

Use this when Qt is installed via Homebrew.

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix)/opt/qt"
```

### Configure

Use this generic form when your environment already exposes Qt to CMake.

```bash
cmake -S . -B build
```

### Configure Without GUI

Use this when Qt is not installed or when working only on the emulator core.

```bash
cmake -S . -B build -DCORE85_BUILD_GUI=OFF
```

### Build

```bash
cmake --build build
```

### Run Tests With Shortcut Target

After the initial configure, this is the shortest way to build and run tests.

```bash
cmake --build build --target check
```

### Run Tests

This remains the direct `ctest` form if you want it explicitly.

```bash
ctest --test-dir build --output-on-failure
```

### Run GUI With Shortcut Target

After a GUI-enabled configure, this builds and launches the Qt app.

```bash
cmake --build build --target run
```

### Clean Reconfigure

Use this when CMake cache state is stale or dependencies changed.

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix)/opt/qt"
cmake --build build
```

## Core Design Rules

- `src/core/` must remain Qt-free.
- Public CPU behavior should align with the interface described in the SRS.
- New emulator behavior should come with tests in `tests/`.
- Prefer deterministic state transitions over hidden side effects.
- Avoid raw `new` and `delete` in new code.

## File Ownership Guide

- `src/core/cpu.*`
  - instruction decoding
  - register state
  - flag behavior
  - cycle counting
  - interrupt handling
- `src/core/memory.*`
  - byte-level memory access
  - ROM protection
  - bulk program loading
- `src/core/io_bus.*`
  - input and output port behavior
  - output callbacks
- `src/core/assembler.*`
  - tokenization
  - symbol resolution
  - directive handling
  - code generation
- `src/gui/*`
  - UI layout
  - widgets
  - event wiring from GUI to core
- `tests/*`
  - regression coverage for emulator behavior

## Recommended Development Order

Follow the milestone order from the SRS unless there is a strong reason to deviate.

1. Expand the CPU execution engine.
2. Add tests for each new opcode family before or alongside implementation.
3. Complete cycle counting and flag correctness.
4. Add interrupt behavior.
5. Implement the assembler.
6. Wire the GUI to the stable core API.

## How To Add A New Instruction

1. Read the expected 8085 behavior from the SRS and datasheet reference you are using.
2. Add or update tests that assert:
   - register state
   - flag state
   - memory side effects
   - cycle count
3. Implement opcode decode and execution in `src/core/cpu.cpp`.
4. Re-run the tests.
5. Confirm the instruction behaves correctly at boundary values, not just a happy path.

## Test Strategy

Current tests focus on foundation behavior.

- `tests/test_cpu.cpp`
  - registers
  - flags
  - reset behavior
  - state snapshots
  - minimal stepping behavior
- `tests/test_memory.cpp`
  - default values
  - address boundaries
  - ROM protection
  - bulk loads
- `tests/test_io.cpp`
  - port defaults
  - port updates
  - output callbacks

Longer term:

- add one or more tests per opcode
- add interrupt coverage
- add assembler parsing and output tests
- add integration tests that load and run short programs

## Troubleshooting

### CMake is missing

Install CMake `3.25+` and re-run the configure step.

### Qt is not installed

Configure with:

```bash
cmake -S . -B build -G Ninja -DCORE85_BUILD_GUI=OFF
```

This still allows core library and test work.

Use the shorter day-to-day commands after that:

```bash
cmake --build build
cmake --build build --target check
```

### GoogleTest fetch fails

Likely causes:

- no network access during configure
- corporate proxy restrictions
- transient GitHub availability problems

Workarounds:

- retry configure later
- vendor `googletest` locally
- point CMake at a preinstalled GTest package if the project is updated to support it

### Tests compile but instruction behavior fails

Check these areas first:

- flag bit semantics
- little-endian byte ordering for 16-bit values
- `PC` increment timing
- taken vs not-taken cycle counts
- whether memory writes should be blocked by ROM guards

### GUI builds but does little

That is expected right now. The Qt app is only a scaffold at this stage.

### The `run` target is missing

The `run` target is only created when:

- `CORE85_BUILD_GUI=ON`
- Qt 6 Widgets is found during configure

If either is missing, reconfigure with a valid Qt path. On macOS with Homebrew:

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix)/opt/qt"
```

## Release Readiness Checklist

Before calling a milestone complete:

- implementation matches the SRS scope for that phase
- tests cover the newly introduced behavior
- the core remains Qt-free
- warnings are addressed on the active compiler
- documentation is updated if workflows changed

## Immediate Next Actions

If you are picking up work from the current state, start here:

1. Implement additional data-transfer and arithmetic opcodes in `CPU::step()`.
2. Add helper routines for flag calculation and common byte or word operations.
3. Expand tests to cover opcode execution and exact cycle counts.
4. Keep the GUI untouched until the core execution model is more complete.
