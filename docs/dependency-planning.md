# Dependency-planning increment

Status: approved, implemented, and verified.

The configuration/listing increment is implemented and verified; see
[its results](verification.md).

## Outcome

Show the order in which a selected task and its prerequisites would run, without
executing anything. Proposed command: `tatr [--file PATH] plan TASK`.

## Approved steps (completed)

1. Extend each task with an optional `depends` array of task-name strings. Require
   every referenced task to exist. Reject duplicate references, empty names, and
   incorrect types. Keep `exec`, `cwd`, and `env` unsupported.
2. Validate missing dependencies and dependency cycles throughout the file before
   producing output, including for `list`. Report the task/reference responsible
   for a missing dependency and the cycle path for a cycle.
3. Add a separate dependency planner. Visit dependencies in their declared order,
   emit each task once, and put the requested task last. Use bytewise UTF-8 names
   consistently with existing configuration and listing behavior.
4. Add `plan TASK` to the CLI. Print one task name per line with exit code 0;
   unknown targets or invalid input return 2 with no partial plan on stdout.
   Preserve existing help, version, file selection, and list behavior.
5. Check chains, shared dependencies, independent branches, duplicate references,
   missing tasks, cycles, unknown targets, and malformed CLI arguments. Retain
   existing checks and verify deterministic output in Windows and Linux builds.
6. Update documentation and examples, record release size changes, and present the
   diff and results for review. Commit/push only when authorized for this increment.

## Scope boundary

No process execution, shell integration, environment overrides, working-directory
changes, parallelism, or caching. File discovery and exit-code conventions stay as
currently documented. Adding commands and native process execution needs a later,
separately reviewed increment.
