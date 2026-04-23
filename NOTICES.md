# Core85 Notices

## Project Notice

Core85 is distributed as a C++17 application and library with a Qt 6 desktop frontend.

## Qt Notice

Core85 is intended to use the Qt LGPL edition with dynamic linking for redistribution scenarios that require LGPL compliance. When shipping binary packages, bundle only the Qt runtime pieces required by the application.

Redistributors are responsible for:

- complying with the applicable Qt license terms
- preserving Qt copyright and license notices
- making any required reverse-linking or relinking rights available when LGPL obligations apply

## Third-Party Components

### Qt 6

- usage: GUI framework and runtime deployment support
- upstream: The Qt Company and contributors
- expected license path: refer to the installed Qt distribution used to build the package

### GoogleTest

- usage: development and test-only dependency
- upstream: GoogleTest contributors
- note: not required for end-user runtime packages

## Packaging Guidance

- keep Qt runtime deployment minimal
- do not bundle development-only tools into end-user packages
- verify package contents and package size during release smoke tests
