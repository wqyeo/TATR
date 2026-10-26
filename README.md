# That's Another Task Runner

TATR is a lightweight task runner in development for teams that need repeatable local
workflows without dedicated CI/CD infrastructure or runners. It aims to replace
the orchestration inside sprawling shell scripts with readable TOML, while keeping
scripts available for the actual work.

The chosen foundation is C++23, targeting Linux, macOS, and Windows. The core
priorities are a lean implementation, minimal dependencies, and a small executable
suited to small systems.

TATR validates a TOML task file, lists tasks, plans dependency order, and executes
commands sequentially, stopping on the first failure.

## Use

```sh
tatr --help
tatr --version
tatr --file examples/tatr.toml list
tatr --file examples/tatr.toml plan check
tatr --file examples/tatr.toml run check
```

`tatr list` reads `tatr.toml` in the current directory. It does not search parent
directories. Tasks are listed alphabetically; invalid configuration fails before
any listing is printed. An empty tasks table is valid.

```toml
version = 1

[tasks.build]
description = "Build the project"
depends = ["configure"]
exec = ["cmake", "--build", "--preset", "dev"]
cwd = "."

[tasks.configure]
exec = ["cmake", "--preset", "dev"]
cwd = "."
```

Tasks support optional `description`, `depends`, `exec`, `cwd`, and `env` fields. `plan TASK`
prints prerequisites in their declared order, each once, with the target last.
`run TASK` executes that order; tasks without `exec` group their prerequisites.
All commands validate the entire configuration first. `list` and `plan` execute
nothing. See [configuration and CLI rules](docs/configuration.md).

Commands are argument arrays, with no implicit shell or expansion. Programs inherit
your environment and standard I/O. Working directories are inherited unless `cwd`
is provided; relative values resolve from the configuration file's directory.
The checked-in example uses `cwd = ".."` to run from the repository root regardless
of the caller's directory. Exit codes are 0 for success, 1 for a failed child (original status
reported on stderr), and 2 for configuration, CLI, or launch errors.

An executable task can override selected inherited environment variables:

```toml
env = { BUILD_MODE = "release", EMPTY_VALUE = "" }
```

Values remain literal strings. Overrides apply only to that child; later tasks
retain their own inherited environment. PATH overrides affect executable lookup.

## Build

Requirements: CMake 3.25 or newer, a compiler supporting C++23, and a C17 compiler
for the vendored TOML parser. On Windows,
install the Visual Studio C++ build tools. On Linux and macOS, use an appropriate
GCC or Clang toolchain and a supported CMake build tool.
POSIX builds require a libc with a child-directory spawn action; CMake checks
for the standard function or its `_np` variant.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Run `build/dev/tatr` with a single-configuration generator, or
`build/dev/Debug/tatr.exe` with Visual Studio. The exact executable suffix and
configuration directory depend on the platform and generator.

For a release build optimized for size:

```sh
cmake --preset release
cmake --build --preset release
```

Optional: `mise run build`, `mise run test`, and `mise run release` wrap these commands. The
repository's `mise.toml` pins CMake; a C++ compiler must be installed separately.

For development checks, use `mise run strict`, `mise run quality`, and
`mise run sanitize`. Formatting and analysis tools are pinned in an optional
Podman image; see [the local workflow and CI proposal](docs/development.md).

The TOML parser is vendored at a pinned revision; building a checkout requires no
dependency downloads. Install with `cmake --install build/release --config MinSizeRel
--prefix <destination>`; this installs the executable and the parser's MIT license.

Windows debug/release and Linux x64 release builds have been verified. macOS is
unverified. See [verification and size measurements](docs/verification.md) for
toolchains, commands, runtime dependencies, and limits of those measurements.

## Project layout

- `src/main.cpp`: native command-line entry point and error reporting.
- `src/cli.*`: CLI parsing and output.
- `src/config.*`: configuration model and validation.
- `src/environment.hpp`: portable override names and environment map type.
- `src/planner.*`: dependency validation and deterministic planning.
- `src/runner.*`: sequential task execution and failure reporting.
- `src/process.*`: native Windows/POSIX process launching and waiting.
- `third_party/tomlc17/`: pinned parser source, provenance, and license.
- `tests/`: CLI checks using CTest and CMake, without a test framework dependency.
- `CMakeLists.txt` and `CMakePresets.json`: build and installation setup.
- `mise.toml`: optional developer tooling and build tasks.
- `examples/tatr.toml`: supported configuration example.
- [Design notes](docs/design.md): problem statement, principles, and open decisions.
- [Parser and schema decisions](docs/configuration-decisions.md): approved decisions and comparison.
- [Dependency-planning increment](docs/dependency-planning.md): completed approved scope.
- [Command-execution increment](docs/command-execution.md): completed approved scope.
- [Safety-hardening increment](docs/safety-hardening.md): completed approved scope.
- [Working-directory increment](docs/task-working-directories.md): completed approved scope.
- [Environment increment](docs/task-environment.md): completed approved scope.
- [Development checks](docs/development.md): local gates and proposed CI workflow.
- [Next increment](docs/next-increment.md): proposal awaiting review.

Each increment is planned and reviewed before implementation. Runtime task
arguments, parallelism, and caching remain future work.
