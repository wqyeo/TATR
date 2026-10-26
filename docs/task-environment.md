# Task environment increment

Status: approved, implemented, and verified.

The user approved the six steps below. See [the configuration contract](configuration.md)
and [verification results](verification.md) for final behavior and evidence.

## Outcome

Let each executable task override selected environment variables with an optional
`env` table, preserving inherited values for all other variables and never
modifying TATR's own environment.

## Proposed steps

1. Add optional `env` as a table of string values, including empty values. Require
   portable variable names matching `[A-Za-z_][A-Za-z0-9_]*`; reject NUL values,
   wrong types, and env fields on grouping tasks. Validate every task before any
   execution, including unrelated tasks.
2. Merge overrides into an owned snapshot of the inherited environment for each
   task. Treat names as case-sensitive on POSIX and case-insensitive on Windows;
   reject conflicting case variants within a Windows task's overrides. Leave
   unspecified values inherited, and keep later tasks/TATR's environment unchanged.
3. Extend native launching with owned POSIX envp storage and a Windows UTF-16
   environment block. Preserve native inherited entries and RAII cleanup, report
   encoding/size/launch failures as exit 2, and do not include variable values in
   TATR diagnostics.
4. Make executable lookup use the effective child PATH, including overridden PATH
   and relative/empty entries under the task's effective directory. Preserve
   existing behavior when env is absent. Document unset/empty PATH differences;
   retain explicit interpreter requirements and no implicit shell expansion.
5. Add meaningful tests for inheritance, replacement, empty/Unicode values,
   isolation across tasks, Windows name casing, invalid unrelated fields, PATH
   lookup with cwd, launch failures, and read-only list/plan. Run native Windows,
   Linux GCC/Clang, formatting/static analysis, and sanitizer checks; report macOS
   limits.
6. Update examples, CLI/configuration docs, and release-size measurements. Present
   results for review; commit/push only when authorized.

## Scope boundary

No environment deletion syntax, dotenv/secrets-file loading, interpolation,
global environment mutation, parallelism, caching, retries, timeouts,
cancellation/process-tree management, new production dependency, or remote CI changes.

## Implementation decisions

- Optional owned map on Task distinguishes an absent env field from an empty table.
  Shared ASCII name validation is in environment.hpp. Inherited names retain their
  native form; only override names use the portable identifier restriction.
- Windows snapshots GetEnvironmentStringsW under RAII, preserves hidden drive
  entries, compares names with case-insensitive CompareStringOrdinal, and builds
  a sorted, double-NUL-terminated UTF-16 environment block.
- POSIX copies native entries once, replaces selected names, and builds envp only
  after all owned string mutations. The environment owner cannot be copied/moved.
- Overridden PATH uses direct POSIX spawn attempts and explicit Windows lookup;
  relative and empty entries use the child's effective directory. A non-PATH
  override preserves the previous lookup policy, and no implicit shell is added.
- Windows overridden entries are limited to 32,767 UTF-16 code units including
  name, separator, and terminator. POSIX retains OS argument/environment limits.
