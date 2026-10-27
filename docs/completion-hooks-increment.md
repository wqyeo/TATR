# Run-completion hooks increment

Status: approved and implemented locally. See [the contract](completion-hooks.md)
and [verification](verification.md). SMTP requires separate approval.
The completed workflow plan is preserved in
[workflow-verification-increment.md](workflow-verification-increment.md).
Existing changes remain uncommitted; commit/push requires authorization.

## Direction

Deliver deployment notifications in two separately reviewed increments:

1. Generic success/failure/completion hooks, with explicit failure policy.
2. Structured SMTP actions through an external curl executable; TATR prepares
   messages and transport configuration, eliminating email wrapper scripts.

Only increment 1 is proposed for the next implementation approval. It does not
compose or send SMTP emails itself. The [SMTP roadmap](notifications.md) describes
increment 2; built-in SMTP/TLS remains deferred.

## Full next-increment plan

1. Add optional root `[[hooks]]` entries: unique `name`, required `when`
   (`success`, `failure`, `always`), optional `targets`, required direct `exec`,
   optional `cwd`, `env`, `env_file`, and `on_error` (`warn` default, or `fail`).
   Validate the complete schema before execution; reuse existing argument, path,
   environment and dotenv rules. Hooks are commands, not task references, and
   have no dependencies. Omitted targets means every requested target; specified
   targets must be nonempty, unique, existing task names. Match the requested
   target, not individual prerequisites.
2. Represent the original task-run result explicitly: status 0/1/2, outcome,
   failed task when known, native exit/signal when available and elapsed duration.
   Keep normal tasks fail-fast. Select matching hooks once from this result;
   execute each once in TOML order, including grouping-target runs. Hook failures
   neither change selection nor prevent other matching hooks from being attempted.
   Hooks do not trigger hooks.
3. Run hooks after execution begins, including task launch/cwd/dotenv failures.
   Exclude help/version/list/plan, CLI/config/graph/target errors, rejected runtime
   arguments and logger initialization failures. After a logging sink fails,
   launch no more commands and return 2, preserving existing behavior. No hook
   guarantee for interruption, crashes or forced termination. Default warn reports
   hook failures without changing status. With fail, an originally successful run
   returns 1 for hook child failure or 2 for preparation/launch failure; 2 wins
   if both occur. Preserve an already failing original task-run status and
   diagnostic, except the existing logging-failure override to 2.
4. Supply child-only context: `TATR_TARGET`, `TATR_OUTCOME` (`success`,
   `child_failure`, `runner_error`), `TATR_STATUS`, `TATR_FAILED_TASK`,
   `TATR_CHILD_EXIT_CODE`, `TATR_CHILD_SIGNAL`, and `TATR_DURATION_MS`.
   Unavailable details are empty strings. Values describe the original task run;
   these reserved names override inherited, dotenv and inline hook values.
   Do not forward runtime arguments to hooks or add general interpolation.
5. Log distinct hook_started/hook_finished events with name, outcome, duration
   and native status. Report ignored failures even when logging is off. Emit the
   final run record after hooks, distinguishing original task outcome from a
   required-hook failure. Preserve task events, inherited child I/O and exclusion
   of argv/environment values from logs. Use small value types, explicit results
   and shared process/environment preparation; no event bus, global state,
   virtual plugin hierarchy or new production dependency.
6. Test matching/filtering, order, grouping/shared dependencies, original-result
   selection, command/launch/cwd/dotenv failures, context precedence, argument
   isolation, warn/fail status precedence, continuing after failed hooks, logging
   off/sink failures, invalid schemas and read-only list/plan. Verify unchanged
   behavior for old configs. Run Windows strict checks and available Linux GCC,
   Clang/static-analysis and ASan/UBSan checks; report macOS unverified without
   native evidence. No benchmarks or new test framework.
7. Update configuration, usage and help; add a portable completion-hook example,
   verification results and an Unreleased changelog entry. Explain that hooks are
   synchronous best-effort commands, not rollback or guaranteed delivery, and
   ordinary commands still lack runner-enforced timeouts. Present results and a
   concrete SMTP increment for separate review. No commit/push/publication without
   authorization.

## Proposed configuration (not supported yet)

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

The executable names above are placeholders, not bundled tools. This illustrates
hook selection, not SMTP configuration. Existing list/plan stdout formats remain
unchanged: plan shows ordinary task order, not conditional hooks. Both commands
validate hook settings without reading dotenv files or launching commands.

## Scope boundary

Only run-completion command hooks in the next increment. No SMTP implementation,
per-task hooks, arbitrary continue-on-error tasks, rollback, retries, parallelism,
general templates, process stdin/capture changes, timeout/cancellation framework,
CI activation, release mutation or performance testing. SMTP remains separately
reviewed so credentials, message formatting and process-I/O changes are concrete
before implementation.
