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

This is experimental alpha software. [Download 0.1.0a from GitLab](https://gitlab.com/wqyeo/thats-another-task-runner/-/releases/0.1.0a)
for Windows x64/MSVC or Alpine Linux 3.21 x64/musl. Each archive includes usage
instructions, an example, a build/file-hash manifest and both MIT notices; SHA256SUMS
is provided alongside the archives. The Alpine artifact needs compatible musl/C++
runtimes. macOS, glibc Linux and other architectures remain unverified. TATR is
[MIT licensed](LICENSE). Source build instructions follow below.

## Use

```sh
tatr --help
tatr --version
tatr --file examples/tatr.toml list
tatr --file examples/tatr.toml plan check
tatr --file examples/tatr.toml run check
tatr --file examples/tatr.toml run check -- --parallel 2
```

These commands assume the built executable is on PATH. Otherwise use its full or
relative path; build and installation locations are shown below.

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

Tasks support optional `description`, `depends`, `exec`, `cwd`, `env`, and `env_file` fields. `plan TASK`
prints prerequisites in their declared order, each once, with the target last.
`run TASK` executes that order; tasks without `exec` group their prerequisites.
All commands validate the entire configuration first. `list` and `plan` execute
nothing. See [configuration and CLI rules](docs/configuration.md).

`run` logs TATR actions to the current console's stderr by default, using one
logfmt-style record per line with UTC timestamps. To also keep an append-only file:

```toml
[logging]
level = "info" # info, warn, error, off
console = true

[logging.file]
directory = "build/logs"
filename = "tatr-{date}-{time}-{pid}.log"
```

The directory resolves from the TOML directory. Filename tokens expand once at
run start: `{date}` is UTC YYYY-MM-DD, `{time}` is HHmmssfffZ (milliseconds), and
`{pid}` is the process ID. Use `tatr.log` for a fixed name, or `tatr-{date}.log`
for daily append. Set `console = false` for file-only logs, or `level = "off"`
to suppress action records. Child stdout/stderr still go directly to the console;
these files contain TATR actions only. There is no rotation/retention, and log
write failures stop further execution with exit 2. See the
[full logging contract](docs/configuration.md#action-logging).

`run TASK -- ARG...` appends literal arguments to that task's configured command.
Prerequisites keep their declared commands. After `--`, flags such as `--file`
belong to the child. Grouping tasks accept no nonempty argument list; rejection
happens before prerequisites run. An empty `--` tail keeps ordinary run behavior.

Commands are argument arrays, with no implicit shell or expansion. Programs inherit
your environment and standard I/O. Working directories are inherited unless `cwd`
is provided; relative values resolve from the configuration file's directory.
The checked-in example uses `cwd = ".."` to run from the repository root regardless
of the caller's directory. Exit codes are 0 for success, 1 for a failed child
(original status in action logs, on stderr by default), and 2 for configuration,
CLI, launch, or logging errors.

An executable task can override selected inherited environment variables:

```toml
env = { BUILD_MODE = "release", EMPTY_VALUE = "" }
```

Values remain literal strings. Overrides apply only to that child; later tasks
retain their own inherited environment. PATH overrides affect executable lookup.

To load a dotenv file, name it explicitly on an executable task:

```toml
[tasks.build]
exec = ["my-build-tool"]
env_file = ".env"
env = { BUILD_MODE = "release" }
```

For example, `.env` could contain:

```dotenv
BUILD_MODE=development
API_URL="http://localhost:8080"
```

Precedence is inherited environment, then file values, then inline `env`; the
example therefore sets BUILD_MODE to release. Relative env_file paths resolve
from the TOML directory, independently of cwd. Files load when the task starts,
after prerequisites; a missing or malformed file stops the workflow with exit 2.
List/plan do not read env files, and no .env file is discovered automatically.

The parser supports comments, optional export, empty values, and single/double
quotes in a documented single-line UTF-8 dialect. Values are not interpolated.
See [dotenv rules](docs/configuration.md#dotenv-files) and the public
[.env.example](examples/.env.example). This repository Git-ignores local `.env`/`.env.*` files;
the example is retained. The checked-in workflow explicitly loads this public
example, so it needs no private env file to run.

## Build destinations and file operations

TATR runs the commands you declare. Your build tool selects its build directory
and artifact names; an install or file-operation task selects the destination.
There are no built-in `build_dir`, `copy`, or `move` actions.

For a CMake project, save a workflow like this in the project's root:

```toml
version = 1

[tasks.configure]
exec = ["cmake", "-S", ".", "-B", "build/output", "-DCMAKE_BUILD_TYPE=Release"]
cwd = "."

[tasks.build]
depends = ["configure"]
exec = ["cmake", "--build", "build/output", "--config", "Release"]
cwd = "."

[tasks.install]
depends = ["build"]
exec = ["cmake", "--install", "build/output", "--config", "Release", "--prefix", "build/stage"]
cwd = "."
```

`tatr run install` builds in `build/output` and installs into `build/stage` according
to that project's CMake install rules. Artifact locations inside a build tree
depend on the project and generator; `--prefix` controls the install root.
Single-configuration generators use `CMAKE_BUILD_TYPE`; Visual Studio and other
multi-configuration generators select Release through `--config Release`.

Ordinary copying and moving also work through external commands. With CMake on
PATH, executable task bodies can use:

```toml
exec = ["cmake", "-E", "copy", "build/app.bin", "build/stage/app.bin"]
# Or, for a move on the same filesystem/volume:
# exec = ["cmake", "-E", "rename", "build/app.bin", "build/stage/app.bin"]
```

Create destination directories first with `cmake -E make_directory`; use
`copy_directory` for directory contents. CMake's `rename` supports files or
directories on one volume. For cross-volume moves, choose a tool with the required
semantics, or a script that copies, verifies success, then removes the source.
Linux/macOS can invoke `mv` directly; Windows PowerShell or cmd built-ins require
an explicit interpreter. The selected tool controls overwrite and error behavior.
See [CMake's command-line tools](https://cmake.org/cmake/help/v4.3/manual/cmake.1.html#run-a-command-line-tool).

Try the self-contained [file-operation example](examples/file-operations.toml):

```sh
tatr --file examples/file-operations.toml plan move
tatr --file examples/file-operations.toml run move
```

It creates demo files only beneath `build/file-demo/`, copies the project license,
and moves the demo source into its output directory. CMake is a dependency of
these example tasks, not a requirement for every TATR workflow.

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

On Linux, CMake's default generator normally needs Make. If you installed Ninja
instead, select it on the first configure:

```sh
cmake --preset dev -G Ninja
cmake --build --preset dev
ctest --preset dev
```

Use the same generator when reusing a build directory. To change it, select a new
build directory or configure with `--fresh`. The release preset has its own build
directory, so select Ninja there too if needed: `cmake --preset release -G Ninja`.
These generator choices do not install the required build tool or compiler.

Run `build/dev/tatr` with a single-configuration generator, or
`build/dev/Debug/tatr.exe` with Visual Studio. The exact executable suffix and
configuration directory depend on the platform and generator.

For a release build optimized for size:

```sh
cmake --preset release
cmake --build --preset release
```

The release preset builds into `build/release`. Common executable locations are:

| Generator | Release executable |
| --- | --- |
| Linux single-configuration Ninja/Makefiles | `build/release/tatr` |
| Windows Visual Studio | `build/release/MinSizeRel/tatr.exe` |

To put the runnable distribution in a chosen directory, from the repository root:

```sh
cmake --install build/release --config MinSizeRel --prefix build/stage
```

With the default install directories, this produces:

```text
build/stage/
  bin/tatr                 # tatr.exe on Windows
  share/tatr/licenses/tatr-LICENSE
  share/tatr/licenses/tomlc17-LICENSE
```

Run the executable from `bin/`, or add that directory to PATH. The executable does
not need the source checkout at runtime. You can copy/move the installed directory
to another location on a compatible system, keeping both license notices when
redistributing it. TOML files, scripts, and programs named by your tasks still need
to be available. Linux binaries require compatible libc/C++ runtimes; a build for
one distribution is not automatically a portable Linux bundle.

For a first invocation from the repository root, use the installed executable
directly; no PATH change is needed:

```sh
# Linux (macOS is not yet verified)
./build/stage/bin/tatr --version
./build/stage/bin/tatr --file examples/tatr.toml run check -- --parallel 2
```

```powershell
# Windows PowerShell
.\build\stage\bin\tatr.exe --version
.\build\stage\bin\tatr.exe --file examples/tatr.toml run check -- --parallel 2
```

The example launches CMake/CTest by name, so those tools must be on PATH. If using
the repository's mise setup, prefix the invocation with `mise exec --`. For a
fresh Linux dev build with Ninja, set `CMAKE_GENERATOR=Ninja` for the invocation,
or configure `dev` with `-G Ninja` first. A preset's cached generator is reused.

Use installation or artifact copying for deployment. CMake build trees contain
cached source/build paths and should not be moved as a substitute for installation.

Optional: `mise run build`, `mise run test`, and `mise run release` wrap these commands. The
repository's `mise.toml` pins CMake; a C++ compiler must be installed separately.

For development checks, use `mise run strict`, `mise run quality`, and
`mise run sanitize`. Formatting and analysis tools are pinned in an optional
Podman image; see [the local workflow and CI proposal](docs/development.md).

The TOML parser is vendored at a pinned revision; building a checkout requires no
dependency downloads. Installation includes the executable and both MIT notices.

To build and verify the local release archives, see [packaging instructions](docs/packaging.md).

Windows debug/release and Linux x64 release builds have been verified. macOS is
unverified. See [verification and size measurements](docs/verification.md) for
toolchains, commands, runtime dependencies, and limits of those measurements.

## Project layout

- `src/main.cpp`: native command-line entry point and error reporting.
- `src/cli.*`: CLI parsing and output.
- `src/config.*`: configuration model and validation.
- `src/environment.hpp`: portable override names and environment map type.
- `src/dotenv.*`: bounded dotenv file loading and parsing.
- `src/logging.*`: synchronous action records and console/file output.
- `src/planner.*`: dependency validation and deterministic planning.
- `src/runner.*`: sequential task execution and failure reporting.
- `src/process.*`: native Windows/POSIX process launching and waiting.
- `third_party/tomlc17/`: pinned parser source, provenance, and license.
- `tests/`: CLI checks using CTest and CMake, without a test framework dependency.
- `CMakeLists.txt` and `CMakePresets.json`: build and installation setup.
- `mise.toml`: optional developer tooling and build tasks.
- `examples/tatr.toml`: supported configuration example.
- `examples/.env.example`: public dotenv values for the workflow example.
- `examples/file-operations.toml`: runnable copy/move example using CMake.
- [Design notes](docs/design.md): problem statement, principles, and open decisions.
- [Parser and schema decisions](docs/configuration-decisions.md): approved decisions and comparison.
- [Dependency-planning increment](docs/dependency-planning.md): completed approved scope.
- [Command-execution increment](docs/command-execution.md): completed approved scope.
- [Safety-hardening increment](docs/safety-hardening.md): completed approved scope.
- [Working-directory increment](docs/task-working-directories.md): completed approved scope.
- [Environment increment](docs/task-environment.md): completed approved scope.
- [Runtime-argument increment](docs/runtime-arguments.md): completed approved scope.
- [Dotenv increment](docs/dotenv-files.md): completed approved scope.
- [Logging increment](docs/action-logging.md): completed approved scope.
- [Release checklist](docs/release-checklist.md): first-use evidence and remaining decisions.
- [Packaging](docs/packaging.md): local archive builds, verification and publication procedure.
- [Development checks](docs/development.md): local gates and proposed CI workflow.
- [Next increment](docs/next-increment.md): proposal awaiting review.

Each increment is planned and reviewed before implementation. First-use workflows
have been exercised on Windows and Alpine Linux; the checklist records remaining
release decisions. Performance benchmarking is deferred;
parallelism and caching remain future work.

## License

TATR is licensed under the [MIT License](LICENSE), copyright 2026 Yeo Wen Qin.
The vendored tomlc17 parser retains CK Tan's separate
[MIT license](third_party/tomlc17/LICENSE) and [provenance](third_party/tomlc17/README.md).
