# Task workflows

The repository workflow commands below assume a source checkout, with CMake/CTest
on PATH. Run them from the checkout root or pass an absolute --file path.

```sh
tatr --help
tatr --version
tatr --file examples/tatr.toml list
tatr --file examples/tatr.toml plan check
tatr --file examples/tatr.toml run check
tatr --file examples/tatr.toml run check -- --parallel 2
```

These commands assume the built executable is on PATH. Otherwise use its full or
relative path; see [build and installation](building.md).

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
`run TASK` executes that order; tasks with neither `exec` nor `smtp` group their prerequisites.
All commands validate the entire configuration first. `list` and `plan` execute
nothing. See [configuration and CLI rules](configuration.md).

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
[full logging contract](configuration.md#action-logging).

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
See [dotenv rules](configuration.md#dotenv-files) and the public
[.env.example](../examples/.env.example). This repository Git-ignores local `.env`/`.env.*` files;
the example is retained. The checked-in workflow explicitly loads this public
example, so it needs no private env file to run.

[Documentation index](README.md)

## Completion commands

TATR 0.2.0a supports optional success/failure/always hooks after the ordinary
task run. They preserve its result by default, can require notification success,
and receive child-only result context. See [completion hooks](completion-hooks.md)
for configuration, eligibility and failure rules. Both hooks and [structured SMTP](smtp.md) are available in 0.2.0a.

```sh
tatr --file examples/completion-hooks.toml run demo
tatr --file examples/completion-hooks.toml run failure
```

The second example deliberately exits 1 after printing its failure summary.
Both use an installed CMake executable; no SMTP service is involved.

## SMTP email

Use a structured smtp action for an explicit email task or completion hook.
TATR prepares the message and invokes an SMTP-capable external curl executable;
no wrapper script is needed. See [SMTP settings and examples](smtp.md). Sending
requires your configured server, credentials where applicable and verified TLS.
Older 0.1.0a archives remain unchanged.
