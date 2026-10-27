# Next increment proposal: first-use and release readiness

Status: awaiting review. Do not implement until approved. The action logging
increment is [implemented and verified](action-logging.md), alongside the earlier
dotenv changes; these changes remain uncommitted. Commit/push needs authorization.
The original [first-use proposal](first-use-release-readiness.md) is preserved.

## Outcome

Establish whether the existing runner is straightforward to build, install and use
in real workflows before adding features. Performance benchmarking remains deferred.

## Full proposed plan

1. Walk through a clean-checkout build, chosen-prefix installation and first task
   invocation on Windows/Linux using documented prerequisites. Record undocumented
   steps; keep macOS unverified without native access.
2. Use an installed TATR for this repository's configure/build/test workflow and
   opt-in developer checks, avoiding rebuilding the executable in use. Check PATH,
   config selection, cwd/env/env_file, runtime arguments, console/file logs and errors.
3. Exercise a small sample project's build/install workflow and the file-operation
   example with ordinary and spaced paths. Keep generated data under build/ and
   verify output/install locations and copied/moved file contents.
4. Prioritize concrete usability issues and correct documentation within this scope.
   Propose runtime behavior changes separately for review; add no speculative features.
5. Write a first-release checklist covering tested toolchains/platforms, compatibility
   gaps, binary/runtime dependencies, package contents, both MIT notices and copyable
   getting-started instructions. Distinguish tested versions from minimum support.
6. Present evidence, remaining blockers and the next practical fix for review.
   Commit/push only when authorized; do not publish a release or activate CI.

## Scope boundary

No performance framework/gates, new execution features, production dependencies,
remote CI activation, release publication or unverified platform/device claims.
