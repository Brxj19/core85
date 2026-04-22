# Core85

Core85 is a cross-platform Intel 8085 emulator and simulator being built in modern C++17 with a Qt 6 desktop frontend. The project is driven by the requirements in [docs/Core85_SRS_v1.0.docx](docs/Core85_SRS_v1.0.docx), with a phased roadmap covering the emulator core, instruction set, assembler, GUI, and packaging.

## Current Status

The project is in the early backend phase.

- `core85_lib` has been scaffolded as a standalone core library with no Qt dependency.
- The CPU foundation includes register state, flag packing/unpacking, reset behavior, cycle tracking, and safe stepping support for `NOP` and `HLT`.
- The memory subsystem includes a 64 KB address space, `0xFF` default values, ROM protection, and bulk program loading.
- The I/O bus includes 256 input ports, 256 output ports, and output callbacks.
- Initial unit tests cover CPU state, memory behavior, ROM protection, and I/O behavior.
- The assembler and GUI are present as early placeholders and will be expanded in later phases.

## Goals

- Emulate the Intel 8085 accurately enough for instruction-by-instruction learning and experimentation.
- Keep the emulator core portable and independent from UI concerns.
- Provide a desktop simulator with a code editor, register viewer, memory viewer, and I/O widgets.
- Support an integrated assembler and direct loading of assembly programs into memory.

## Repository Layout

```text
Core85/
├── CMakeLists.txt
├── README.md
├── runbook.md
├── docs/
│   └── Core85_SRS_v1.0.docx
├── src/
│   ├── core/
│   │   ├── assembler.cpp
│   │   ├── assembler.h
│   │   ├── cpu.cpp
│   │   ├── cpu.h
│   │   ├── io_bus.cpp
│   │   ├── io_bus.h
│   │   ├── memory.cpp
│   │   ├── memory.h
│   │   └── span.h
│   └── gui/
│       ├── components/
│       ├── main.cpp
│       ├── mainwindow.cpp
│       ├── mainwindow.h
│       └── mainwindow.ui
└── tests/
    ├── test_cpu.cpp
    ├── test_io.cpp
    └── test_memory.cpp
```

## Requirements

- CMake `3.25+`
- A C++17 compiler
  - Clang `15+`
  - GCC `12+`
  - MSVC 2022
- Qt `6.x` for the GUI target
- Internet access during configure time if `googletest` is fetched through CMake `FetchContent`

## Build

Configure the project:

```bash
cmake -S . -B build
```

Build everything:

```bash
cmake --build build
```

Build only the core library and tests:

```bash
cmake -S . -B build -DCORE85_BUILD_GUI=OFF
cmake --build build
```

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

## Architecture Notes

- `src/core/` is the emulator backend and must remain free of Qt dependencies.
- `src/gui/` is the Qt frontend and should talk to the core only through the public CPU API.
- The SRS defines the long-term contract for the public API, milestones, and acceptance criteria.

## Near-Term Roadmap

1. Expand CPU instruction decoding and execution beyond `NOP` and `HLT`.
2. Implement ALU and flag behavior for arithmetic, logical, and branch instructions.
3. Grow the unit test suite toward instruction-by-instruction coverage.
4. Implement the two-pass assembler defined in the SRS.
5. Replace the GUI placeholders with the full simulator interface.

## Development Notes

- Treat the SRS as the source of truth for scope and behavior.
- Favor small, testable increments in the core before building UI features on top.
- Keep the core deterministic and easy to test in isolation.

For day-to-day engineering workflows, see [runbook.md](runbook.md).
