# Run-completion hooks

Available in 0.2.0a; verification results are recorded in
[verification.md](verification.md). [SMTP actions](smtp.md) can also be used in hooks.

Hooks let TOML describe success/failure handling without a script wrapping the
whole deployment. See the runnable
[example](../examples/completion-hooks.toml).

```toml
version = 1

[tasks.deploy]
exec = ["deployment-tool", "apply"]

[[hooks]]
name = "deployment-notification"
when = "always"
targets = ["deploy"]
exec = ["notification-client", "send"]
env_file = ".env.notifications"
on_error = "warn"
```

The command names here are placeholders; TATR does not bundle either tool or
bundle an SMTP client. A structured smtp action can prepare email instead of exec;
see [SMTP configuration](smtp.md). Invoke an installed notification client directly when
its interface fits; structured SMTP support will remove email-specific glue.

## Configuration

Root `hooks` is an optional array of tables. An empty array is valid. Entries
have only these fields; unknown fields and wrong types are errors:

| Field | Contract |
| --- | --- |
| name | Required, nonempty, unique, no C0/DEL/C1 controls; same name restrictions as tasks. Hook names occupy a separate namespace. |
| when | Required: success, failure, or always. |
| targets | Optional nonempty array of distinct existing task names; omitted means every requested target. |
| exec / smtp | Exactly one action: a nonempty direct argument array, or [structured SMTP settings](smtp.md). |
| cwd | Optional child directory, resolved from the TOML directory; absent inherits caller cwd. |
| env | Optional child-only string environment overrides, using task rules. |
| env_file | Optional explicit dotenv path, resolved from the TOML directory and read immediately before the hook starts. |
| on_error | Optional warn (default) or fail. |

All hooks are validated before any task runs, including hooks that will not match.
A prerequisite can create a hook directory or dotenv file. Hooks have no depends
field and do not name other tasks. Runtime arguments apply only to the requested
task, never to its hooks. No shell or string interpolation is added.

list/plan validate hooks but do not read their dotenv files, inspect directories,
resolve executables or launch commands. Their stdout formats stay unchanged; plan
lists ordinary task order, not conditional hooks.

## Selection and result policy

Ordinary tasks remain sequential and stop on the first failure. TATR saves that
original result before running hooks. Match against the requested target, not
individual prerequisites, and execute each eligible hook once in declaration
order. Grouping targets also get completion hooks, including empty groups.
Success means original status 0; failure means original status 1 or 2; always
matches both. A hook failure never changes matching or triggers more hooks.
Remaining matching hooks are attempted after an earlier hook fails.

| Original task status | Hook policy/result | Final status |
| --- | --- | --- |
| 0 | Hooks succeed, or failures use warn | 0 |
| 0 | A fail-policy hook child fails | 1 |
| 0 | A fail-policy hook preparation/launch fails | 2; takes precedence over hook child failure |
| 1 or 2 | Any hook result | Original task status and diagnostic preserved |
| Any | Logging sink fails | 2; no further commands launch |

Warn-policy failures produce warning records. If the configured level filters the
record (including off), a minimal escaped stderr diagnostic still reports the
failure. Required-hook failures use error records and the same fallback rule.
An enabled file-only sink receives its records without an additional console
message. A fallback stderr write failure is a logging error.

Hooks cover task child failures and task preparation/launch/cwd/dotenv errors.
They do not run for CLI/config/schema/graph/target errors, rejected runtime
arguments, help/version/list/plan or logger initialization failure. Logging
failure immediately stops additional launches, even if hooks have not run.

## Child context

TATR sets these variables for hook children only, overriding inherited, dotenv
and inline values of the same names (case-insensitively on Windows):

| Variable | Value |
| --- | --- |
| TATR_TARGET | Requested target, including a grouping target. |
| TATR_OUTCOME | Original success, child_failure, or runner_error. |
| TATR_STATUS | Original runner status 0, 1, or 2. |
| TATR_FAILED_TASK | First failed task, or empty on success. |
| TATR_CHILD_EXIT_CODE | Original failed child's native code, or empty if unavailable/terminated by signal. |
| TATR_CHILD_SIGNAL | Original failed child's POSIX signal, or empty if unavailable. |
| TATR_DURATION_MS | Task-run duration through completion of ordinary tasks, excluding hooks, measured with the steady clock. |

Every hook gets the same original context. No raw exception text, arguments,
environment values or captured output are inserted into context. Direct-command hook child
stdout/stderr remain inherited and can themselves print sensitive information.
SMTP-client streams are suppressed as described in the [SMTP guide](smtp.md).
The parent and later ordinary task environments are not mutated.

## Logs and limitations

Executable tasks retain task_started/task_finished. Hooks use hook_started and
hook_finished with a quoted hook field, outcome, native status when available
and duration including dotenv loading. Hook failures report child_failure or
runner_error. run_finished comes after all hooks; its duration includes hooks.
When any hooks are configured, it also includes task_outcome for the original
result. If tasks succeeded but a required hook failed, overall outcome is
hook_failure, with the determining hook diagnostic. Otherwise existing run
outcomes remain. Ignored failures do not change the final success outcome.

Hooks are synchronous best-effort commands. There is no timeout, retry, rollback,
parallelism, per-task hook or delivery spool. A hung hook can hold up the run;
use client-specific timeouts where available. Crashes, interruption and forced
termination may prevent hooks or the final record entirely. Existing completed
side effects remain after any failure.
