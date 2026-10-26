# Sequential command-execution increment

Status: approved, implemented, and verified.

Dependency planning is implemented; see [the completed plan](dependency-planning.md)
and [verification results](verification.md).

## Outcome

Run a selected task and its prerequisites with `tatr [--file PATH] run TASK`.
Keep the current `list` and `plan` commands read-only.

## Approved steps (completed)

1. Add optional `exec` arrays of argument strings. Require a nonempty array and a
   nonempty executable name; reject wrong types and embedded NUL characters.
   Permit empty subsequent arguments. A task without `exec` groups prerequisites
   and succeeds after they complete.
2. Add a small native process interface: POSIX process spawning for Linux/macOS
   and the wide-character Windows process API. Resolve executable names through
   PATH, preserve argument boundaries, and document platform lookup differences.
   Validate Windows quoting with a test helper that reports its actual arguments.
3. Implement `run TASK` using the existing planner. Validate the whole configuration
   before launching anything; run reachable tasks sequentially, once each, in plan
   order. Skip process creation for grouping tasks.
4. Inherit the caller's working directory, environment, stdin, stdout, and stderr.
   Execute argument arrays directly. Shell scripts must explicitly name their
   interpreter; do not add implicit shell expansion or built-in command handling.
5. Stop on the first failed task. Propose exit 0 for success, 1 for a child's
   nonzero exit or signal termination, and 2 for configuration/CLI/spawn failures.
   Report the task name and original child exit status or signal on stderr.
   Document basic interruption behavior; process-tree cancellation remains deferred.
6. Test real child processes for argument fidelity (spaces, quotes, backslashes,
   Unicode, and empty arguments), inherited I/O, ordering, grouping tasks, shared
   prerequisites, spawn failures, and stopping on failure. Verify invalid
   configurations launch nothing. Retain existing checks and verify Windows/Linux;
   report macOS verification limits.
7. Update help, examples, configuration docs, and release-size measurements.
   Present results for review. Commit/push only when authorized.

## Scope boundary

No `cwd` or `env` overrides, parallelism, caching, retries, timeouts, task arguments,
implicit shell, or process-tree management. Bring material portability or behavior
changes back for review before implementation proceeds beyond the approved plan.
