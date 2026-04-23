# Core85 Release Notes v1.0

## Summary

Core85 v1.0 delivers a complete educational Intel 8085 emulator and simulator desktop workflow:

- full 8085 execution engine with interrupt support
- assembler with labels, directives, and Intel HEX support
- Qt debugger GUI with stepping, breakpoints, memory/register inspection, and teaching peripherals
- packaging and install support through CMake and CPack

## Included in v1.0

### Emulator Core

- standalone `core85_lib` with no Qt dependency
- complete register, flag, memory, and I/O modeling
- instruction execution coverage validated through unit and matrix tests

### Assembler

- two-pass assembly pipeline
- labels, `ORG`, `EQU`, `DB`, `DW`, `END`
- Intel HEX import/export helpers
- source mapping for editor and debugger correlation

### GUI

- onboarding welcome screen and sample program flow
- multi-tab editor with syntax highlighting and breakpoint gutter
- find/replace, go-to-line, and Problems panel
- register viewer with multiple display modes
- memory viewer with jump/search/bookmark helpers
- I/O panel with switch inputs, LED output, and 2-digit 7-segment display

### Packaging

- install rules for app, library, headers, examples, and documentation
- CPack configuration for release artefacts on macOS, Windows, and Linux
- CI packaging smoke-test coverage

## Validation

Release validation should include:

- project build success
- full GTest suite pass
- successful `package` target generation
- smoke verification that the packaged artefact opens or extracts correctly

## Notes

- Version tag creation should happen after committing the release snapshot
- package size and bundled Qt runtime should be reviewed during release packaging
