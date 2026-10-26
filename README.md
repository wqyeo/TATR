# That's Another Task Runner

TATR is a planned lightweight task runner for teams that need repeatable local
workflows without dedicated CI/CD infrastructure or runners. It aims to replace
the orchestration inside sprawling shell scripts with readable TOML, while keeping
scripts available for the actual work.

The chosen foundation is C++23, targeting Linux, macOS, and Windows. The core
priorities are a lean implementation, minimal dependencies, and a small executable
suited to small systems.

This repository currently contains only the project scaffold. The executable
prints a placeholder message; TOML parsing and task execution are not implemented.

## Build

Requirements: CMake 3.25 or newer and a compiler supporting C++23. On Windows,
install the Visual Studio C++ build tools. On Linux and macOS, use an appropriate
GCC or Clang toolchain and a supported CMake build tool.

```sh
cmake --preset dev
cmake --build --preset dev
```

Run `build/dev/tatr` with a single-configuration generator, or
`build/dev/Debug/tatr.exe` with Visual Studio. The exact executable suffix and
configuration directory depend on the platform and generator.

For a release build optimized for size:

```sh
cmake --preset release
cmake --build --preset release
```

Optional: `mise run build` and `mise run release` wrap the same commands. The
repository's `mise.toml` pins CMake; a C++ compiler must be installed separately.

## Project layout

- `src/main.cpp`: placeholder executable.
- `CMakeLists.txt` and `CMakePresets.json`: build and installation setup.
- `mise.toml`: optional developer tooling and build tasks.
- `examples/tatr.toml`: illustrative configuration proposal.
- [Design notes](docs/design.md): problem statement, principles, and open decisions.
- [Next increment](docs/next-increment.md): implementation proposal awaiting review.

The intended first user workflow is to list tasks from a TOML file, then inspect
their dependency order before execution is introduced. Configuration and CLI
details remain proposals until reviewed.
