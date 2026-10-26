# Configuration and CLI

TATR supports configuration validation, task listing, dependency planning, and
sequential execution. Configuration input and TATR's own output are UTF-8;
child programs control their own output encoding. The parser accepts TOML 1.1; the root
`version` field is TATR's schema version, a separate concept.

## File format

Configuration files may contain at most **1 MiB (1,048,576 bytes)**, including
comments, whitespace, and any encoding marker. Larger files fail with exit 2
before parser-input allocation or task execution. This bounds source input size;
it does not cap total parser/graph memory or child-process resources.

```toml
version = 1

[tasks.build]
description = "Build the project"
exec = ["cmake", "--build", "--preset", "dev"]

[tasks.check]
depends = ["build"]

[tasks."build.debug"]
description = "A quoted name containing a dot"
```

- `version` is required and must be the integer `1`. Strings, floats, booleans,
  missing values, and other integers are rejected.
- `tasks` is required and must be a table. An empty `[tasks]` table is valid.
- Each task must be a table with a nonempty name. Quote names containing dots;
  unquoted dots create nested TOML tables, rather than a single task name.
- The optional string `description` describes a task. An absent or empty
  description produces just the name when listing.
- The optional `depends` array contains task-name strings in prerequisite traversal
  order. An absent or empty array means no prerequisites. References must name
  existing tasks; duplicate references within one array and cycles are rejected.
  Forward references are allowed. Shared dependencies across tasks are allowed.
- Task names, dependency names, and descriptions must contain no C0, DEL, or C1 control characters,
  including escaped newline, tab, NUL, and terminal escape characters. Unicode
  text is otherwise supported.
- Dependency names must be nonempty, just like task names.
- The optional `exec` array contains argument strings, starting with a nonempty
  executable name. Explicitly empty arrays, wrong types, and embedded NULs are
  rejected. Subsequent arguments may be empty and may contain other control
  characters such as tabs/newlines. An absent `exec` makes a grouping task.
- Unknown root/task fields are rejected. `cwd` and `env` are
  not supported by this increment. TOML duplicate keys are parse errors.

## Commands

```text
tatr
tatr --help
tatr --version
tatr [--file PATH] list
tatr [--file PATH] plan TASK
tatr [--file PATH] run TASK
```

With no arguments, print usage and return success. `--help` and `--version` are
standalone invocations; neither opens a task file. Extra options or arguments in
those invocations are rejected.

All three commands default to `tatr.toml` in the current working directory. There is no search
in parent directories. `--file PATH` may appear before or after the command or target, at most
once. Empty paths and an option supplied where a path is expected are rejected.
For a filename beginning with `--`, use an explicit path such as `./--tasks.toml`.

Tasks are listed in bytewise UTF-8 key order, one per line:

```text
build - Build the project
build.debug - A quoted name containing a dot
check
```

`plan TASK` prints task names only, one per line. It visits prerequisites depth
first in the order declared in each `depends` array, includes each reachable task
once, and ends with the requested task. Unrelated tasks are omitted. For example,
`plan check` above prints `build` then `check`. Descriptions are omitted.
Unknown targets, a missing target, and extra arguments are errors. Names beginning
with `-` cannot be passed as CLI targets; there is no `--` separator in this increment.

The complete file is validated before listing/planning or launching any child, including
missing dependencies and cycles in unrelated tasks. Errors do not produce a
partial list or plan. Cycle diagnostics show the cycle path. Errors go to stderr,
prefixed by `tatr:`. Configuration
errors include the path and, where available, a source line/column or field name.
`list` and `plan` never launch a child, even when tasks contain `exec` fields.

## Sequential execution

`run TASK` follows the same order as `plan TASK`. Each reachable task is processed
once; tasks without `exec` succeed after their prerequisites and launch no process.
The runner waits for each child to finish before starting the next task. Unknown
targets and malformed arguments fail before any child runs.

Children inherit the caller's working directory, environment, stdin, stdout, and
stderr. The configuration's directory does not become the working directory;
relative executable/script paths are relative to the caller. Child output goes
directly to the inherited streams; TATR adds no progress text to stdout. Output
already produced by completed or failed children remains visible on failure.

The array is executed directly. No variable expansion, globbing, pipes,
redirection, or command substitution is provided. An interpreter can be selected
explicitly, for example `["/bin/sh", "script.sh"]`,
`["cmd.exe", "/d", "/c", "script.cmd"]`, or
`["pwsh", "-NoProfile", "-File", "script.ps1"]`. These commands are
platform-specific, and the interpreter controls its own quoting and expansion.

Exit codes:

- `0`: every selected task succeeded, including empty grouping tasks.
- `1`: a child exited nonzero or was terminated by a signal. Stderr reports the
  failing task and original status/signal. Later tasks are skipped.
- `2`: CLI, configuration, executable lookup, process launch, or wait failure.
  Later tasks are skipped. Earlier tasks may already have run when a launch fails;
  commands are not pre-launched to check their availability.

Windows preserves the full unsigned 32-bit child status in diagnostics. A child
that exits 2 still makes TATR exit 1. On POSIX implementations that report a
post-spawn exec failure through child status 127, TATR treats it as a child failure.

## Platform execution rules and interruption

POSIX uses `posix_spawnp` with inherited environment and file descriptors. Names
without `/` use PATH; names with `/` identify a path directly. No shell fallback
is added for executable text without an interpreter. The OS may itself recognize
executable scripts with a shebang. The spawn/wait contract is described by
[POSIX](https://pubs.opengroup.org/onlinepubs/9799919799/functions/posix_spawn.html).

Windows uses `SearchPathW` (default extension `.exe`) and checks for a native
executable image before `CreateProcessW`, passing the resolved executable
explicitly. Native lookup includes system/application locations and PATH; its
current-directory ordering depends on Windows search policy. It does not implement
PATHEXT expansion or App Paths lookup. Batch and PowerShell files require an
explicit interpreter. See [SearchPathW](https://learn.microsoft.com/en-us/windows/win32/api/processenv/nf-processenv-searchpathw)
and [CreateProcessW](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw).

Windows arguments use [Microsoft CRT quoting rules](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments).
Programs with their own parsers, including `cmd.exe`, can interpret command-line
text differently; use their documented syntax. The encoded Windows command line
must fit within 32,767 UTF-16 code units including its terminator. TATR closes its
process and duplicated standard handles after each child completes. Windows uses
an explicit handle allowlist so other inheritable handles held by TATR are not
passed to children. If no standard handles exist, handle inheritance is disabled.

No signal or console-control handlers, new process groups, or Windows Job Objects
are installed. Normal OS behavior applies: terminal Ctrl+C can reach both TATR and
the foreground child; the runner itself may terminate before printing a diagnostic.
Sending a signal only to TATR does not ensure its child stops. Cancellation and
descendant cleanup are not managed in this increment. Waiting retries POSIX EINTR.
