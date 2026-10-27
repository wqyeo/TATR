# Action logging increment

Status: implemented and verified on 2026-10-27; committed in `01566dd`.
See [verification results](verification.md).

Logging took priority at the user's request; the approved
[first-use/release-readiness increment](first-use-release-readiness.md) followed it.
Earlier dotenv work was committed alongside this increment.

## Convention

Use synchronous, dependency-free, logfmt-style UTF-8 key=value records: one record
per line, fixed field order, no colors. Timestamp format follows
[RFC 3339](https://www.rfc-editor.org/rfc/rfc3339.html), with UTC milliseconds.
Severity meanings follow informational/warning/error terminology from
[RFC 5424](https://www.rfc-editor.org/rfc/rfc5424.html). This is TATR's text
convention, not the syslog wire protocol or a claim of universal logfmt compliance.

```text
time=2026-10-27T08:00:00.123Z level=info event=run_started target="check"
time=2026-10-27T08:00:00.124Z level=info event=task_started task="build"
time=2026-10-27T08:00:01.456Z level=info event=task_finished task="build" outcome=success exit_code=0 duration_ms=1332
time=2026-10-27T08:00:02.000Z level=info event=run_finished target="check" outcome=success duration_ms=1877
```

Levels: info, warn, error, off; default info. One shared threshold controls both
sinks. No debug/trace events in this increment. Quote task, target, and message
strings; escape quotes, backslashes, newline/tab/CR and remaining C0/DEL/C1 controls.
Keep numeric fields unquoted and keys/event names fixed ASCII. Invalid UTF-8 in
native diagnostics must become explicit escaped bytes, not malformed log text.

## Proposed configuration

```toml
version = 1

[logging]
level = "info"
console = true

[logging.file]
directory = "build/logs"
filename = "tatr-{date}-{time}-{pid}.log"

[tasks.check]
exec = ["ctest", "--test-dir", "build"]
```

- No logging table: action records on stderr at info, no log file. list/plan stdout
  retains its existing format. Child stdout/stderr remain inherited.
- Optional logging.file enables a file in addition to the console. Its directory
  defaults to logs; filename defaults to tatr-{date}-{time}-{pid}.log.
- console=false supports file-only logging. Without a file it requires level=off,
  preventing accidental silent configurations. off creates no files/directories;
  operational errors still receive a minimal stderr diagnostic and normal exit code.
- Resolve directory from the TOML directory, independently of task cwd. Absolute
  directories are supported; reject NUL and Windows drive-relative paths.
- filename is a portable basename: reject separators, controls, Windows-forbidden
  characters, reserved device names, trailing dots/spaces, dot/dot-dot, unknown
  tokens and malformed braces. Validate template and expanded result. Unknown
  TOML fields/types are errors.
- Only run creates directories/opens logs, after full config, graph, target and
  grouping/runtime-argument validation, before any prerequisite. list/plan validate
  logging settings without touching log paths. Help/version do not read TOML.
- Append, never truncate. Files use UTF-8 without BOM and LF endings. Console/file
  receive the same TATR records. Check every write/flush and final flush/close;
  destructor-only cleanup cannot report finalization failure.

## Filename tokens

Expand once at run start from one UTC snapshot:

| Token | Meaning | Example |
| --- | --- | --- |
| {date} | YYYY-MM-DD | 2026-10-27 |
| {time} | HHmmssfffZ, fff = milliseconds | 080000123Z |
| {pid} | Decimal native TATR process ID | 1234 |
| {{ and }} | Literal braces | { and } |

Examples: tatr.log, tatr-{date}.log (daily append), and the default above (ordinary
run separation). No task-name/env/strftime interpolation or directory tokens.
Names are not guaranteed unique: reused names append. Concurrent processes writing
the same path may interleave records; no cross-process atomicity/locking guarantee.
The file remains open across midnight. No rotation, retention, compression, or
fsync/durable-storage guarantee is included.

## Events and failure behavior

- Emit run_started/run_finished for valid runs, including grouping-only runs.
  Emit task_started/task_finished for executable tasks, shared prerequisites once.
  Unexecuted tasks produce no lifecycle records.
- task_started precedes env_file loading; its duration includes loading and child
  execution. Use steady_clock for nonnegative duration_ms; system_clock only for
  timestamps/names. Record native exit status/signal where applicable.
- Outcomes distinguish success, child failure and runner error, retaining CLI exit
  codes 0/1/2. Include escaped useful errors. Do not also print a caught exception
  as a plain diagnostic if the logger already reported it. Task outcomes and the
  overall run summary are distinct records.
- Never log argument arrays, runtime arguments, environment values, dotenv contents,
  or environment blocks. Task names/error paths may disclose project metadata;
  this is not a general secret-redaction guarantee.
- A configured file create/open error returns 2 before tasks launch, with direct
  stderr fallback even if console=false. No silent fallback to console-only.
- Mid-run sink failure returns 2 and prevents the next task. Failed task_started
  logging prevents that task launching; failed task_finished logging occurs after
  child completion. Completed effects remain. Report sink failures through direct
  stderr without recursive logging. If stderr fails too, still return 2. No new
  child interruption or background logging is introduced.
- CLI/TOML/target failures before logger initialization keep existing stderr errors
  and create no log files. level=off still reports failures through stderr.
- Log files contain TATR actions only. Child stdout/stderr pass through to the
  console. Capturing/teeing child output requires a separate process-I/O proposal;
  ordering between child output and TATR records is not guaranteed.

## Approved implementation plan

1. Specify and test the contract: optional root logging table, defaults, strict
   schema validation, levels, event fields, escaping, filename tokens, path anchoring,
   and read-only list/plan behavior. Document the new default console progress and
   scope of child output.
2. Add a small synchronous logger with scoped file ownership, console/file sinks,
   deterministic stream/time seams, filename rendering and checked writes/flush/close.
   Use value types/composition; no logging dependency, global mutable state, worker
   thread or virtual sink framework.
3. Integrate through an explicit logger reference in CLI/runner. Open files before
   prerequisites; emit run/task lifecycle, native outcomes and durations; centralize
   exception reporting. Preserve process I/O and exit semantics. Fix the reviewed
   help/version output-error bypass while making CLI output failures consistent.
4. Add focused tests for console defaults, stream separation, filtering/off/file-only,
   success/failure/launch/env_file/grouping/shared dependencies, escaping and sensitive
   value exclusion; tokens/invalid basenames, append, UTC snapshot consistency,
   spaced/Unicode/config-relative paths and directory creation. Verify no filesystem
   effects for list/plan/off/invalid runs. Inject stream failures to verify fallback,
   checked finalization, exit 2 and prevention of subsequent launches; use a file
   as a parent directory for deterministic open failures. Update existing stderr
   assertions for progress records. All generated fixtures stay under build/.
5. Update README, configuration reference, public example and verification notes
   with copyable settings, naming rules, failure/privacy/child-output limitations,
   and lack of rotation/retention. Preserve the first-use/release-readiness proposal.
6. Run Windows strict tests, Linux Clang quality/static analysis, Linux ASan/UBSan
   and GCC build/tests; verify installed-binary console/file examples. Report macOS
   unverified without native evidence. Present results and limitations. Commit/push
   only when separately authorized.

## Scope boundary

Only the plan above is authorized. No child-output capture, async logging, new
production dependency, JSON formatter, remote/syslog transport, per-task sinks,
rotation/retention framework, benchmarks, CI activation or release publication.
