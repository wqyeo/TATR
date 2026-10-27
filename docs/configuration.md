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
  characters such as tabs/newlines. A task with neither `exec` nor `smtp` is a group.
- Optional `smtp` selects a structured email action instead of `exec`; see
  [SMTP configuration](smtp.md). Both fields together are invalid. SMTP tasks
  require explicit body/body_file and do not accept runtime arguments.
- The optional `cwd` is a nonempty directory string without NUL. It requires an
  command or SMTP action; grouping tasks cannot specify a process directory.
- The optional `env` table overrides selected inherited environment variables for
  an executable task; values must be strings and may be empty. Names must match
  `[A-Za-z_][A-Za-z0-9_]*`. NUL values and env fields on grouping tasks are rejected.
- The optional `env_file` names one nonempty dotenv path without NUL; it requires
  an executable task. Relative paths resolve from the TOML directory. Absolute
  paths are accepted; Windows drive-relative paths are rejected.
- Optional root `logging` configures action records, as described below.
- Optional root `hooks` configures run-completion command or SMTP actions; see [completion hooks](completion-hooks.md).
- Unknown root/task/hook/logging fields are rejected. TOML duplicate keys are parse errors.

## Action logging

`run` emits synchronous logfmt-style UTF-8 `key=value` records on stderr by default.
`list`/`plan` keep their existing stdout formats and emit no action records.
Logging adds no library, thread, color or shell. SMTP separately uses a scoped
stdin writer and an external client; see its transport rules.

```toml
[logging]
level = "info"
console = true

[logging.file]
directory = "build/logs"
filename = "tatr-{date}-{time}-{pid}.log"
```

All settings are optional. Without `[logging]`, the defaults are info-level
console logging and no file. A `[logging.file]` table enables file logging,
defaulting to directory `logs` and filename `tatr-{date}-{time}-{pid}.log`.
Its only supported fields are nonempty string `directory` and `filename`.
`console` must be boolean. `level` accepts `info`, `warn`, `error`, or `off`,
in increasing severity. A shared threshold applies to both sinks. TATR emits
informational lifecycle events, error outcomes and warnings for ignored hook
failures. `warn` preserves warnings and errors; `error` suppresses warning action
records (hook failures still get a stderr diagnostic). Filtered logging may
create an empty log file.

Set `console = false` for file-only records. Without a file, console=false
requires level=off. `off` creates no log directories/files and suppresses action
records; operational errors still receive a minimal stderr diagnostic.

Directories resolve relative to the supplied TOML file's directory, independently
of task cwd; absolute paths work too. NUL and Windows drive-relative paths are
rejected. Only a valid `run` creates missing directories and opens the file,
after complete schema/graph/target/runtime-argument validation and before any
prerequisite. `list`/`plan` validate the settings/template without accessing log
paths. Help/version do not read the configuration. Initialization failures return
2 before any task launches, with a direct stderr diagnostic even for file-only
settings; TATR does not silently switch to console-only.

Filename templates must be portable basenames. Separators, C0/DEL/C1 controls,
Windows-forbidden characters (`<>:"/\\|?*`), reserved device names (including
CON, PRN, AUX, NUL, COM1–9, LPT1–9 and their superscript-digit equivalents),
trailing dots/spaces, dot/dot-dot, unknown tokens and malformed braces are rejected.
The expanded name is checked too. Unicode and ordinary spaces are accepted.

| Token | UTC expansion at run start | Example |
| --- | --- | --- |
| `{date}` | YYYY-MM-DD | `2026-10-27` |
| `{time}` | HHmmssfffZ, milliseconds | `080000123Z` |
| `{pid}` | Decimal native process ID | `1234` |
| `{{` / `}}` | Literal braces | `{` / `}` |

Tokens use one snapshot, expanded once; a file stays open across midnight.
`tatr.log` appends to one file; `tatr-{date}.log` appends by UTC start date.
Names are not guaranteed unique, including the default. Existing files are never
truncated. Concurrent processes sharing a filename may interleave records; there
is no locking/atomic-record guarantee. No task/env/strftime interpolation,
directory tokens, rotation, retention, compression, or durable-storage/fsync
guarantee is included.

Records use UTC millisecond timestamps following RFC 3339, with severity meanings
following RFC 5424's informational/warning/error terminology. This is TATR's text
convention, not syslog wire format or universal logfmt parser compatibility.
Fields have a fixed order; task/target/message strings are always quoted.
Quotes, backslashes, newline/CR/tab and remaining C0/DEL/C1 controls are escaped;
invalid UTF-8 diagnostic bytes become explicit `\xhh` escapes. Numeric fields are
unquoted. Files are UTF-8 without BOM with LF endings. Both sinks receive the same
TATR records, with checked writes/flushes and explicit file finalization.

```text
time=2026-10-27T08:00:00.123Z level=info event=run_started target="check"
time=2026-10-27T08:00:00.124Z level=info event=task_started task="build"
time=2026-10-27T08:00:01.456Z level=info event=task_finished task="build" outcome=success exit_code=0 duration_ms=1332
time=2026-10-27T08:00:02.000Z level=info event=run_finished target="check" outcome=success duration_ms=1877
```

Valid runs, including grouping-only runs, produce run_started/run_finished.
Completion hooks have separate lifecycle events and final-result rules documented
in [completion hooks](completion-hooks.md). Only executable tasks produce
task_started/task_finished; shared prerequisites
appear once and skipped tasks have no records. task_started precedes dotenv
loading. Steady-clock durations include dotenv loading and child execution;
wall-clock time is used only for timestamps/names. Task outcomes are `success`,
`child_failure`, or `runner_error`. Completed children include their native
`exit_code`, or POSIX `signal` on signal termination. Failure records include an
escaped `message`; task outcomes and the run summary are distinct records.
Already-logged exceptions do not produce an additional plain diagnostic.

Argument arrays/runtime arguments, environment values/blocks and dotenv contents
are not logged. Task names and diagnostic paths can disclose project metadata;
this is not a general secret-redaction guarantee. Child stdout/stderr remain
inherited by the console and are not captured/teed into log files. Children can
print their own secrets. Ordering across child streams and action records is not
guaranteed.

A sink failure returns 2 and prevents the next task or hook. Failed task_started logging
prevents launch; failed task_finished logging occurs after the child completed.
Earlier side effects remain. Sink failures use direct stderr fallback without
recursive logging; if stderr also fails, exit status remains 2. Children are not
interrupted. Pre-initialization CLI/config/target errors retain stderr diagnostics
without creating log files. Ordinary exit codes remain 0/1/2.

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
partial list or plan. Cycle diagnostics show the cycle path. These validation errors
go to stderr prefixed by `tatr:`. Configuration errors include the path and, where
available, a source line/column or field name. Failures after logging starts follow
the action-logging rules above.
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
once; tasks with neither `exec` nor `smtp` succeed after their prerequisites
and launch no process. SMTP tasks use the [SMTP transport contract](smtp.md).
The runner waits for each child to finish before starting the next task. Unknown
targets and malformed arguments fail before any child runs.

Direct exec children inherit stdin, stdout, and stderr, and the environment except for declared
`env_file`/`env` overrides. Without `cwd`, they
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
- `1`: a child exited nonzero or was terminated by a signal. Action logs report the
  failing task and original status/signal (stderr by default, or a minimal stderr
  diagnostic with level=off). Later tasks are skipped.
- `2`: CLI, configuration, working-directory, executable lookup, process launch,
  wait, or logging failure.
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
executable lookup. Values undergo no interpolation or shell expansion. Files are
loaded only through explicit env_file fields as described below. TATR never
changes its own environment; prerequisites cannot pass new
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

### Dotenv files

```toml
[tasks.build]
exec = ["my-build-tool"]
env_file = ".env"
env = { BUILD_MODE = "release" }
```

Only one explicit file is supported per executable task. There is no automatic
discovery, global dotenv scope, optional-file mode, or env-file array. Paths resolve
from the supplied TOML path's directory, independently of task cwd. Files must be
readable regular files; symlinks to regular files are accepted. The loader does
not canonicalize their paths.

The task file schema is validated for all tasks before execution. A dotenv file
is opened and parsed immediately before its task launches, after prerequisites,
allowing a prerequisite to generate it. Each task reads its own file at that time;
there is no cross-task file cache. List/plan do not open env files or check their
availability; unrelated tasks' files are not read. Missing/unreadable files and
parse failures stop execution with exit 2; earlier side effects remain. Invalid
files still fail when inline env would override their values.

Precedence is **inherited environment < file values < inline env**. Inline values
may intentionally replace a file value, including differently cased Windows
names. POSIX names remain case-sensitive. The file layer uses the same portable
identifier rule as inline env, and duplicate names within a file are errors
(case-insensitive on Windows). Empty files and empty values are valid. Effective
PATH values from either layer use the existing child-directory-aware lookup.
TATR and subsequent tasks' environments are never mutated.

TATR's dotenv dialect is deliberately explicit:

- Files must be valid UTF-8 with an optional initial UTF-8 BOM. Only LF or CRLF
  line endings are accepted; a final newline is optional. NUL is rejected anywhere.
  Each file may contain at most 1 MiB (1,048,576 bytes), checked before allocating
  the input buffer. This is a per-file input limit, not a total memory budget.
- Blank lines and lines beginning with # after spaces/tabs are ignored. An
  optional `export` token followed by spaces/tabs is accepted before an assignment.
- Assignments split at the first `=`. Spaces/tabs around the name and immediately
  after `=` are ignored. Names must match `[A-Za-z_][A-Za-z0-9_]*`.
- Unquoted values retain internal whitespace and quotes, with outer spaces/tabs
  trimmed. A # starts a comment at value start or after a space/tab; an adjacent
  # is literal (`NAME=a#b` sets `a#b`).
- A single/double quote at the first nonspace value position delimits the value.
  Single quotes preserve literal bytes, including backslashes and #. Double
  quotes support only `\\`, `\"`, `\n`, `\r`, and `\t` escapes. Unknown escapes
  are errors. After closing quotes, only spaces/tabs or a # comment are allowed.
- Physical multiline quoted values, concatenated quoted fragments, and line
  joining are unsupported. Escaped newlines inside double-quoted values are
  supported. `$NAME`, `${NAME}`, `%NAME%`, and command substitutions remain literal;
  the file is never executed as a shell script.

Errors identify the task and file, with a line number where available, without
printing assignment lines or values. File reads are not an atomic snapshot:
truncation/read failures or detected growth are errors, but concurrent file edits
are not otherwise serialized. Use a completed file before launching the task.

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
