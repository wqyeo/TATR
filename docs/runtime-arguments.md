# Runtime task-argument increment

Status: approved, implemented, and verified.

The user approved the six steps below. See [the configuration contract](configuration.md)
and [verification results](verification.md) for final behavior and evidence.

## Outcome

Allow `tatr run TASK -- ARG...` to append literal arguments to the requested task's
declared command, leaving prerequisite commands unchanged.

## Proposed steps

1. Add `run TASK -- ARG...`; accept the separator only after a run target. Treat
   every following token literally, including empty strings, flags, and --file.
   A separator with no arguments retains ordinary run behavior. Preserve option
   parsing before it and reject unsupported list/plan uses or extra positional
   arguments without a separator.
2. Pass an immutable view of runtime arguments from CLI to runner. Append them
   after the target's configured exec arguments in an owned temporary vector;
   never mutate the configuration or append arguments to prerequisites.
3. Reject nonempty runtime arguments on grouping targets with exit 2 before any
   prerequisite executes. Keep existing dependency order, once-only execution,
   cwd/env behavior, native argument encoding, and failure-status rules.
4. Retain direct execution: no argument splitting, interpolation, globbing, or
   implicit shell. Apply existing native command-size/launch checks to the combined
   command; launch failures stop later tasks and retain prior effects.
5. Test appended argument order, empty/Unicode/spaced values, quotes/backslashes,
   shell metacharacters, flags after the separator, --file placement, prerequisite
   isolation, grouping rejection before side effects, and invalid syntax. Run
   Windows/Linux GCC/Clang, formatting/static analysis, and sanitizer checks;
   report macOS limits and native argument-limit coverage.
6. Update examples, help/configuration docs, and release-size measurements. Present
   results for review; commit/push only when authorized.

## Scope boundary

No named argument schema, task-name syntax expansion, interpolation, dependency
argument forwarding, environment deletion, parallelism, caching, retries, timeouts,
cancellation/process-tree management, new production dependency, or remote CI changes.

## Implementation decisions

- CLI parsing borrows a span of the original argument vector after the separator;
  it stops option parsing immediately, preserving empty strings and literal flags.
- The runner checks grouping-target compatibility before executing any prerequisite.
  It copies and appends only the target's command and only when runtime arguments
  are present. Config and prerequisite arguments remain immutable.
- Native process APIs, cwd/env behavior, shell policy, and exit-status mapping are
  unchanged. A Windows regression verifies a combined command can exceed its size
  limit even when the configured and CLI portions individually fit.
