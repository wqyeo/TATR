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
cwd = "."

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
- The optional `cwd` is a nonempty directory string without NUL. It requires an
  `exec` field; grouping tasks cannot specify a process directory.
- The optional `env` table overrides selected inherited environment variables for
  an executable task; values must be strings and may be empty. Names must match
  `[A-Za-z_][A-Za-z0-9_]*`. NUL values and env fields on grouping tasks are rejected.
- Unknown root/task fields are rejected. TOML duplicate keys are parse errors.

## Commands

```text
tatr
tatr --help
tatr --version
tatr [--file PATH] list
tatr [--file PATH] plan TASK
tatr [--file PATH] run TASK [-- ARG...]
```

With no arguments, print usage and return success. `--help` and `--version` are
standalone invocations; neither opens a task file. Extra options or arguments in
those invocations are rejected.

All three commands default to `tatr.toml` in the current working directory. There is no search
in parent directories. `--file PATH` may appear before or after the command or target, at most
once, before any runtime-argument separator. Empty paths and an option supplied
where a path is expected are rejected.
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
with `-` cannot be passed as CLI targets. The run-only separator described below
does not expand task-name syntax.

The complete file is validated before listing/planning or launching any child, including
missing dependencies and cycles in unrelated tasks. Errors do not produce a
partial list or plan. Cycle diagnostics show the cycle path. Errors go to stderr,
prefixed by `tatr:`. Configuration
errors include the path and, where available, a source line/column or field name.
`list` and `plan` never launch a child, even when tasks contain `exec` fields.

## Sequential execution

### Runtime arguments

`run TASK -- ARG...` appends the supplied strings after the target's declared exec
arguments. For example, a configured `["ctest", "--preset", "dev"]` becomes
`["ctest", "--preset", "dev", "--parallel", "2"]` when invoked with
`run TASK -- --parallel 2`. The caller's shell determines which strings reach
TATR; TATR does no further splitting, globbing, or interpolation.

The separator is accepted only after a run target. Every following token belongs
to the child, including empty strings, `--file`, `--help`, and another `--`.
Options before the separator keep their existing meaning. A separator with no
following tokens retains ordinary run behavior; an explicit empty string is one
argument. Extra positional arguments without the separator, misplaced separators,
and separators for list/plan are rejected with exit 2 before any child executes.

Only the requested target receives runtime arguments. Prerequisites retain their
declared commands, run once in dependency order, and retain their own cwd/env.
Grouping targets reject a nonempty runtime argument list (even one empty string)
with exit 2 before any prerequisites execute. The combined command lives in an
owned temporary vector; the loaded configuration is never mutated.

Native argument encoding and size limits apply to the final combined command.
Launch/size failures return 2 and preserve earlier effects; child failures retain
exit 1. Runtime arguments do not override the executable, cwd, or env fields.

### Execution order and failures

`run TASK` follows the same order as `plan TASK`. Each reachable task is processed
once; tasks without `exec` succeed after their prerequisites and launch no process.
The runner waits for each child to finish before starting the next task. Unknown
targets and malformed arguments fail before any child runs.

Children inherit stdin, stdout, and stderr, and the environment except for declared
`env` overrides. Without `cwd`, they
also inherit the caller's directory. With `cwd`, the rules below apply. Relative
script arguments are interpreted by the child program from its working directory;
TATR does not rewrite argument strings. Child output goes
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
- `2`: CLI, configuration, working-directory, executable lookup, process launch, or wait failure.
  Later tasks are skipped. Earlier tasks may already have run when a launch fails;
  commands are not pre-launched to check their availability.

Windows preserves the full unsigned 32-bit child status in diagnostics. A child
that exits 2 still makes TATR exit 1. On POSIX implementations that report a
post-spawn exec failure through child status 127, TATR treats it as a child failure.

## Platform execution rules and interruption

### Task working directories

Relative `cwd` paths resolve against the directory of the configuration path
supplied to TATR; absolute paths are accepted. `cwd = "."` therefore selects the
configuration directory, while an absent field retains the caller's directory.
Relative `--file` paths are interpreted from the caller as before. A symlink to a
configuration file uses the supplied path's parent, rather than the target file's
parent. TATR does not canonicalize or lexically collapse directory paths, preserving
native symlink/`..` behavior.

Field types are validated for the whole file before any child runs. Directory
availability is checked only when a selected task starts, so prerequisites may
create directories and unrelated tasks may refer to missing directories. Neither
`list` nor `plan` checks directory availability or creates directories. TATR never
creates a missing working directory automatically and never changes its own cwd.
Missing directories, non-directories, permission failures, and launch failures
stop execution with exit 2 and task/directory context. Earlier effects remain;
directory checks cannot prevent a concurrent rename/removal before spawn.

On POSIX, a child-only spawn action applies the requested directory before exec.
CMake checks for `posix_spawn_file_actions_addchdir` and then the `_np` variant;
builds fail clearly if neither is available in the target libc. The current Alpine
toolchain uses `_np`. Older libcs may lack it; macOS remains unverified. See the
[POSIX directory action](https://pubs.opengroup.org/onlinepubs/9799919799/functions/posix_spawn_file_actions_addchdir.html).

With `cwd`, executable paths containing a directory component resolve from the
child directory. On POSIX, bare names use inherited PATH; relative and empty PATH
entries resolve from the child's directory. An unset PATH follows libc defaults.
On Windows, bare names with `cwd` search only effective PATH (inherited unless
overridden, with `.exe` as the
default extension); relative and empty entries are anchored to the child directory.
There is no implicit insertion of application, system, or caller directories in
that mode. Quoted Windows PATH entries are unwrapped. An unavailable/empty Windows
PATH makes bare-name lookup fail; use an explicit executable path instead.
Windows drive-relative forms such as `C:work` are rejected for `cwd`, and for
executable/PATH entries when an explicit `cwd` applies. Use `C:/work` or an ordinary
relative path. Windows directory and PATH names containing `;` remain subject to
the native search-path delimiter rules.

### Task environment overrides

```toml
[tasks.check]
exec = ["ctest", "--preset", "dev"]
cwd = "."
env = { CTEST_OUTPUT_ON_FAILURE = "1", OPTIONAL_VALUE = "" }
```

An env table merges into an owned snapshot of the inherited environment for that
task. Unspecified values are preserved. Empty strings set a variable to an empty
value; no deletion syntax is provided. An empty table is valid on executable
tasks. Field types and names are validated across the whole configuration before
any task runs. Neither list nor plan constructs a child environment or performs
executable lookup. Values undergo no interpolation, shell expansion, or dotenv
loading. TATR never changes its own environment; prerequisites cannot pass new
variables to later tasks by altering their own process environments.

POSIX override names are case-sensitive. Windows names are case-insensitive, and
conflicting variants such as `MODE` and `mode` within one table are rejected on
Windows. Inherited names need not follow the portable override-name restriction:
their native forms, including Windows hidden drive entries, are preserved.
Windows uses a sorted, double-NUL-terminated UTF-16 block with
CREATE_UNICODE_ENVIRONMENT; POSIX uses owned strings and a stable envp array.
See [the Windows environment contract](https://learn.microsoft.com/en-us/windows/win32/procthread/changing-environment-variables).

When PATH is overridden, bare executable names search that effective PATH, using
the task's cwd or the caller's directory if cwd is absent. Relative and empty PATH
entries use that directory. An explicitly empty PATH searches only that directory
on both platforms. Windows matches PATH override names case-insensitively, retains
its `.exe` default, and searches only the effective PATH in this mode. An env
table without a PATH override preserves existing lookup behavior, including
native defaults when the inherited PATH is unset. With explicit cwd on Windows,
an inherited unavailable/empty PATH still fails bare-name lookup as described
above. Explicit executable paths bypass PATH.

POSIX PATH overrides use direct posix_spawn attempts, rather than posix_spawnp's
parent-environment lookup. Missing/non-directory candidates are skipped;
permission-denied candidates allow a later entry to succeed, with EACCES reported
if all remaining candidates are missing. Other launch errors stop the search.
No implicit shell fallback is added for executable script text.

Windows overridden entries, including name, `=`, and terminating NUL, are limited
to 32,767 UTF-16 code units by TATR. POSIX argument/environment limits are enforced
by the OS. Encoding, size, and launch failures return 2 and stop later tasks;
earlier side effects remain. TATR does not print environment values in its
validation or native launch diagnostics. Child programs control their own output.

### Native execution and interruption

POSIX uses `posix_spawnp` or the PATH-override search described above, with native
file-descriptor inheritance and the selected environment. Names
without `/` use PATH; names with `/` identify a path directly. No shell fallback
is added for executable text without an interpreter. The OS may itself recognize
executable scripts with a shebang. The spawn/wait contract is described by
[POSIX](https://pubs.opengroup.org/onlinepubs/9799919799/functions/posix_spawn.html).

Windows uses `SearchPathW` (default extension `.exe`) and checks for a native
executable image before `CreateProcessW`, passing the resolved executable
explicitly. Without `cwd` or a PATH override, native lookup includes system/application locations and PATH; its
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
