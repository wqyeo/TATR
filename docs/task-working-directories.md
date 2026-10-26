# Task working-directory increment

Status: approved, implemented, and verified.

The user approved the six steps below after the safety-hardening increment.
See [the configuration contract](configuration.md) and
[verification results](verification.md) for final behavior and evidence.

## Outcome

Let each executable task choose its working directory with optional `cwd`, while
preserving inherited working-directory behavior when the field is absent.

## Proposed steps

1. Add optional nonempty string `cwd`; reject wrong types and embedded NULs.
   Reject `cwd` on grouping tasks because no process would use it.
2. Resolve relative `cwd` against the configuration file's directory; accept
   absolute paths. Without `cwd`, retain the caller's working directory. Do not
   create directories automatically or change TATR's own working directory.
3. Extend the native process interface to apply the directory only to its child.
   Keep the lean dependency policy. Resolve relative executable paths against the
   effective child directory and document PATH lookup behavior on both platforms.
4. Validate field types for the whole configuration before execution. Check that
   each requested working directory exists when its task starts, allowing an
   earlier prerequisite to create it. Report directory/launch failures with task
   and path context, stop execution, and return 2.
5. Test inherited, absolute, relative, Unicode, and spaced paths; relative --file
   paths; missing directories; prerequisite-created directories; independent task
   directories; executable lookup; and read-only list/plan behavior. Retain all
   existing checks and verify Windows/Linux, reporting macOS limits.
6. Update examples, help/configuration docs, and release-size measurements. Present
   results for review; commit/push only when authorized.

## Scope boundary

No environment overrides, interpolation, automatic directory creation, parallelism,
caching, retries, timeouts, or cancellation/process-tree management.

## Implementation decisions

- Store an optional owned filesystem path on each task. Resolve it from the
  supplied config path without canonicalization, preserving symlink/.. behavior.
- POSIX spawn actions use the standard addchdir function when available, otherwise
  `_np`; CMake checks support. Windows uses CreateProcessW's child directory.
- With explicit cwd, Windows bare names search inherited PATH only. Relative and
  empty entries resolve from the child directory. Without cwd, the existing
  native search policy remains. Drive-relative Windows forms are rejected.
- No new production dependency, parent-directory mutation, or remote CI change.
