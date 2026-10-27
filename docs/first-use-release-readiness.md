# Next increment proposal: first-use and release readiness

Status: awaiting review. Do not implement until approved.

The basic runner includes task directories, environment overrides, runtime
arguments, and [explicit dotenv files](dotenv-files.md). The user deferred
performance benchmarking before the first release. MIT licensing is decided;
this previously deferred first-use increment awaits separate approval.

## Outcome

Establish whether the existing runner is useful and straightforward to build,
install, and use in real project workflows before adding more features.

## Proposed steps

1. Walk through a clean-checkout build, chosen-prefix installation, and first
   task invocation on Windows/Linux, using only documented prerequisites. Record
   every undocumented step and keep macOS explicitly unverified without native access.
2. Use an installed TATR copy for this repository's configure/build/test workflow
   and opt-in developer checks, avoiding rebuilding the executable currently in use.
   Record friction around PATH, config selection, cwd/env/env_file, arguments, and errors.
3. Exercise a small sample project's build/install workflow and the file-operation
   example with ordinary and spaced paths. Keep generated data beneath build/;
   verify chosen output/install directories and copied/moved file contents.
4. Record and prioritize concrete usability issues. Make documentation corrections
   within this scope; propose runtime behavior changes separately for review.
   Do not add features merely to complete a checklist.
5. Write a first-release checklist covering tested toolchains/platforms, unresolved
   compatibility gaps, binary/runtime dependencies, package contents, both MIT
   notices, and copyable getting-started instructions. Distinguish tested versions
   from guaranteed minimum support; do not claim unverified macOS support.
6. Present the evidence, remaining blockers, and the next practical fix for review.
   Commit/push only when authorized; do not publish a release or activate CI as
   part of this increment.

## Scope boundary

No performance benchmark framework, new execution features, production dependencies,
automatic performance gates, remote CI activation, release publication, or claims
of native-platform/tiny-device validation without evidence.
