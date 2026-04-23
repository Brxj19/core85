# Core85

Core85 is a cross-platform Intel 8085 emulator and educational simulator built in modern C++17 with a Qt 6 desktop frontend. Version 1.0 includes a Qt debugger-style GUI, a two-pass assembler, Intel HEX support, instruction-level test coverage, and packaging via CMake/CPack.

The project is driven by [docs/Core85_SRS_v1.0.docx](docs/Core85_SRS_v1.0.docx). The emulator core in `src/core/` remains Qt-free and is linked into the GUI from `src/gui/`.

## Version 1.0 Highlights

- complete 8085 execution engine with instruction, flag, and timing validation
- two-pass assembler with labels, directives, and Intel HEX import/export
- debugger-oriented Qt GUI with editor, Problems panel, register view, memory view, and I/O widgets
- onboarding flow, session persistence, recent files/folders, and keyboard-first debugging workflow
- 2-digit 7-segment display, LED panel, and switch input teaching peripherals
- CMake install rules plus archive and installer packaging for macOS, Windows, and Linux release artefacts

## Repository Layout

```text
Core85/
├── CMakeLists.txt
├── README.md
├── RELEASE_NOTES_v1.0.md
├── NOTICES.md
├── runbook.md
├── docs/
│   └── Core85_SRS_v1.0.docx
├── src/
│   ├── core/
│   └── gui/
├── tests/
│   ├── asm/
│   └── test_*.cpp
└── .github/
    └── workflows/
```

## Requirements

- CMake `3.25+`
- C++17 compiler
  - Clang `15+`
  - GCC `12+`
  - MSVC 2022
- Qt `6.x` with `Widgets` for the GUI
- internet access during configure if `googletest` is fetched via `FetchContent`

## Build

### Configure with GUI

```bash
cmake -S . -B build
```

On macOS with Homebrew Qt:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix)/opt/qt"
```

### Configure without GUI

```bash
cmake -S . -B build -DCORE85_BUILD_GUI=OFF
```

### Build

```bash
cmake --build build
```

### Test

```bash
ctest --test-dir build --output-on-failure
```

### Run the GUI

```bash
cmake --build build --target run
```

## Package

Binary packages are produced with CPack after a GUI-enabled configure:

```bash
cmake --build build --target package
```

Platform defaults:

- macOS: `.zip` archive and `.dmg` installer
- Windows: `.zip` archive and `.exe` NSIS installer
- Linux: `.tar.gz` archive and `.deb` package

The install tree includes:

- `core85_gui` application artefact
- `core85_lib` static library
- public core headers
- example assembly programs from `tests/asm/`
- release docs and notices

## Installed Contents

Core85 installs runtime and developer-facing artefacts through standard CMake install directories:

- binaries/bundles to `${CMAKE_INSTALL_BINDIR}` or app bundle root
- static library to `${CMAKE_INSTALL_LIBDIR}`
- core headers to `${CMAKE_INSTALL_INCLUDEDIR}/core85/core`
- documentation to `${CMAKE_INSTALL_DOCDIR}`
- sample programs to `${CMAKE_INSTALL_DATADIR}/core85/examples`

## Testing and Quality

The project currently ships with a large GTest suite covering CPU behavior, assembler behavior, interrupts, memory, I/O, and opcode-matrix validation.

Typical local validation:

```bash
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target package
```

## Documentation

- SRS: [docs/Core85_SRS_v1.0.docx](docs/Core85_SRS_v1.0.docx)
- engineering workflow: [runbook.md](runbook.md)
- release summary: [RELEASE_NOTES_v1.0.md](RELEASE_NOTES_v1.0.md)
- notices and packaging/legal notes: [NOTICES.md](NOTICES.md)
