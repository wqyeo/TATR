# That's Another Task Runner

TATR is a small task runner written in C++23. It reads TOML, validates task
relationships, and runs commands sequentially, stopping at the first failure.
The design prioritizes minimal dependencies and a small executable.

## Why and when to use TATR

Use TATR when you need repeatable local workflows without dedicated CI/CD
infrastructure or runners, and want task orchestration out of a growing shell
script. Named tasks, dependencies, working directories and environment settings
live together in a readable file.

TOML describes orchestration; programs and scripts contain procedural logic.
TATR can help reduce shell scripting, while still letting you call a script when
that is the clearest way to express the work.

A short shell script can be simpler for a straightforward workflow. Ansible
pointing at localhost can also do the job well, especially if it already fits
your team's configuration-management workflow. Use whichever keeps your setup
small and understandable; TATR adds no requirement to operate CI/CD infrastructure.

## Download and status

[Download the experimental 0.2.0a alpha](https://gitlab.com/wqyeo/thats-another-task-runner/-/releases/0.2.0a).

| Published package | Runtime baseline |
| --- | --- |
| Windows x64/MSVC ZIP | Static MSVC runtime; Windows x64 |
| Alpine Linux 3.21 x64/musl tar.gz | Compatible musl, libstdc++ and libgcc runtimes |
| Debian 12 x64/glibc tar.gz | glibc 2.36 baseline, libstdc++ and libgcc runtimes |

Extract the entire archive and run `bin/tatr --version` (PowerShell:
`.\bin\tatr.exe --version`), or add `bin/` to PATH. Archives include their own
usage guide, public examples, changelog, build/file-hash manifest and both MIT notices. Verify the
archive against the release's `SHA256SUMS` file.

The [Debian 12 x64/glibc baseline](docs/glibc-linux.md) is verified. macOS, other glibc baselines,
other architectures and older systems remain unverified. CLI/configuration may
change during alpha development. See the [changelog](CHANGELOG.md) and
[verification evidence](docs/verification.md).

## Quickstart

With TATR on PATH, save this as `tatr.toml` in a demo directory:

```toml
version = 1

[tasks.hello]
description = "Say hello"
exec = ["cmake", "-E", "echo", "Hello from TATR"]
```

Run these commands from that directory:

```sh
tatr list
tatr plan hello
tatr run hello
```

This example needs CMake on PATH. Your own tasks can invoke any available program;
TATR itself needs no compiler or CMake at runtime.

Use `tatr --file /path/to/tatr.toml run hello` from another directory. The default
is `tatr.toml` in the caller's directory, with no parent-directory search.
Commands are literal argument arrays, with no implicit shell or interpolation.

Tasks support `description`, `depends`, `exec` or `smtp`, `cwd`, `env` and `env_file`.
`list` and `plan` validate the whole configuration without running tasks.
`run TASK -- ARG...` appends literal arguments to the target command only.

Action records go to stderr by default; optional TOML settings enable append-only
log files. Direct-command stdout/stderr stay attached to the console; SMTP client output is suppressed. Exit codes are 0 for
success, 1 for a failed child and 2 for configuration/runner/logging errors.
See the [task workflows](docs/usage.md) and [CLI/TOML reference](docs/configuration.md).

## Build

From a source checkout, use CMake 3.25+, a C++23 compiler, a C17 compiler and a
supported build tool. Windows builds need Visual Studio C++ build tools. The TOML
parser is vendored, so the build needs no production-dependency downloads.

```sh
cmake --preset release
cmake --build --preset release
cmake --install build/release --config MinSizeRel --prefix build/stage
```

If using Ninja on Linux, select it on the first configure with
`cmake --preset release -G Ninja`. Run `build/stage/bin/tatr` afterward, or
`build/stage/bin/tatr.exe` on Windows. Keep both installed license notices when
redistributing. Install the distribution instead of moving a CMake build tree.

See [build and installation](docs/building.md) for dev builds, generator choices,
PATH/mise setup and platform requirements.

## Documentation

- [Task workflows and examples](docs/usage.md)
- [Configuration, environment/dotenv and logging](docs/configuration.md)
- [Run-completion hooks](docs/completion-hooks.md) and [SMTP email](docs/smtp.md)
- [Build destinations and copying/moving files](docs/file-workflows.md)
- [Development checks and changelog maintenance](docs/development.md)
- [Packaging](docs/packaging.md) and [release checklist](docs/release-checklist.md)
- [Documentation index and project layout](docs/README.md)

SMTP uses an external SMTP/TLS-capable curl client and supports send timeouts.
There is no parallel execution, caching, general command timeout/process-tree cleanup, child-output
capture or log rotation/retention. Platform-specific commands still need their
respective programs or an explicitly chosen interpreter.

## License

[MIT](LICENSE), copyright 2026 Yeo Wen Qin. The vendored tomlc17 parser retains
CK Tan's separate [MIT notice](third_party/tomlc17/LICENSE) and
[provenance](third_party/tomlc17/README.md).
