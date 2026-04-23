# Core85 Runbook

This runbook is the working guide for building, testing, packaging, and releasing Core85.

## Source of Truth

- product requirements: [docs/Core85_SRS_v1.0.docx](docs/Core85_SRS_v1.0.docx)
- build and packaging config: [CMakeLists.txt](CMakeLists.txt)
- release notes: [RELEASE_NOTES_v1.0.md](RELEASE_NOTES_v1.0.md)
- notices and redistribution notes: [NOTICES.md](NOTICES.md)

## Current V1.0 Snapshot

- `src/core/` is a Qt-free emulator library
- `src/gui/` is a Qt 6 desktop simulator/debugger frontend
- the assembler is implemented and integrated into the GUI
- the GUI supports multi-tab editing, problems, stepping, memory/register inspection, and teaching peripherals
- packaging is driven by CMake install rules and CPack
- CI is expected to build, test, and smoke-package on macOS, Ubuntu, and Windows

## Toolchain

Required:

- CMake `3.25+`
- C++17 compiler
- Qt `6.x` with `Widgets` for GUI builds

Helpful:

- Ninja
- `clang-format`
- `clang-tidy`
- debugger such as `lldb`, `gdb`, or Visual Studio debugger

## Standard Workflows

### Configure with GUI on macOS

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix)/opt/qt"
```

### Configure with GUI on a machine where Qt is discoverable

```bash
cmake -S . -B build
```

### Configure without GUI

```bash
cmake -S . -B build -DCORE85_BUILD_GUI=OFF
```

### Build

```bash
cmake --build build
```

### Run tests

```bash
cmake --build build --target check
```

or

```bash
ctest --test-dir build --output-on-failure
```

### Run the GUI

```bash
cmake --build build --target run
```

### Build a release package

```bash
cmake --build build --target package
```

Generated package types:

- macOS: `.zip`
- Windows: `.zip`
- Linux: `.tar.gz`

### Install to a local prefix

```bash
cmake --install build --prefix ./stage
```

This is useful for smoke-checking install layout before packaging.

## Packaging Rules

Phase 5 requires installable artefacts and smoke-tested packaging. The repo now packages:

- the `core85_gui` desktop app when GUI is enabled
- the `core85_lib` static library
- public core headers under `include/core85/core`
- example assembly programs from `tests/asm`
- release documentation and notices

Packaging is intentionally conservative:

- only required Qt runtime pieces should be bundled
- package names should include version, platform, and architecture
- package size should stay within the SRS target budget

## Release Checklist

Before calling a V1.0 release candidate ready:

- configure succeeds on target platform
- `cmake --build build` passes
- `ctest --test-dir build --output-on-failure` passes
- `cmake --build build --target package` succeeds
- generated package opens or extracts successfully
- README, runbook, notices, and release notes match the shipped behavior
- phase-complete commit message is prepared
- version tag is created after committing the release snapshot

## CI Expectations

The CI matrix should:

- build on macOS, Ubuntu, and Windows
- run the full GTest suite
- run a packaging smoke pass through the `package` target

If packaging breaks on one platform, treat it as a release blocker for Phase 5.

## File Ownership Guide

- `src/core/cpu.*`
  instruction decode, flags, cycles, interrupts
- `src/core/memory.*`
  64 KB memory behavior, ROM protection
- `src/core/io_bus.*`
  256-port I/O model and callbacks
- `src/core/assembler.*`
  parser, directives, source map, Intel HEX
- `src/gui/*`
  Qt workflow, editor, docking, debugger UX, peripherals
- `tests/*`
  regression tests and opcode coverage
- `.github/workflows/*`
  CI build, test, and packaging smoke validation

## Troubleshooting

### Qt is not found

Configure with a valid Qt prefix or disable the GUI:

```bash
cmake -S . -B build -DCORE85_BUILD_GUI=OFF
```

### The `run` target is missing

The GUI target was not created. Check:

- `CORE85_BUILD_GUI=ON`
- Qt 6 Widgets was found during configure

### The `package` target fails

Check:

- GUI-enabled configure was used for app packaging
- Qt deployment support is available from the installed Qt
- install rules are valid via `cmake --install build --prefix ./stage`

### CPack output is larger than expected

Review:

- which Qt modules are being bundled
- whether debug symbols or unnecessary artefacts are included
- whether the build type is appropriate for release packaging

### Settings restore opens stale files

Session restore only reopens file-backed tabs that still exist. If a file moved, open it manually and resave.

## Documentation Hygiene

Whenever workflows or release behavior change, update:

- [README.md](README.md)
- [runbook.md](runbook.md)
- [RELEASE_NOTES_v1.0.md](RELEASE_NOTES_v1.0.md)
- [NOTICES.md](NOTICES.md)

## Release Commands

Typical release smoke flow:

```bash
rm -rf build
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix)/opt/qt"
cmake --build build
ctest --test-dir build --output-on-failure
cmake --build build --target package
```
