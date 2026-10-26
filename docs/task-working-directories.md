# Next increment proposal: task working directories

Status: awaiting review. Do not implement until approved.

Sequential command execution is implemented; see [the completed plan](command-execution.md)
and [verification results](verification.md).

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
