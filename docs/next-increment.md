# Next increment proposal

Status: awaiting review. Do not implement until approved.

## Outcome

Read a task file and list its task names and descriptions. This gives us a small,
reviewable configuration foundation before adding dependency resolution or process
execution.

## Proposed steps

1. Agree the initial CLI: `tatr --help`, `tatr --version`, and
   `tatr [--file PATH] list`. Default to `tatr.toml` in the current directory,
   without searching parent directories.
2. Compare suitable TOML parsers using license, standards support, integration
   cost, and a small release-size measurement. Propose a pinned dependency and
   delivery method for review before adding it.
3. Agree a minimal versioned schema: `version = 1`, named entries under `[tasks]`,
   and an optional string `description`. Decide whether `exec` and `depends` from
   the illustrative example should be validated now or introduced later. Reject
   unsupported fields with an actionable error rather than silently ignoring them.
4. After those decisions are approved, add a small configuration model and parser,
   separate from CLI handling. Report missing files, malformed TOML, unsupported
   versions, and invalid fields clearly.
5. Implement alphabetical task listing. Keep help and version independent of a
   configuration file. Use exit code 0 for success and 2 for CLI/configuration
   errors, subject to review.
6. Add focused checks for successful listing, missing and malformed files,
   unsupported versions, and invalid field types. Exercise the available Windows
   toolchain; validate Linux through Podman if available. Record macOS as unverified
   until a macOS build is available.
7. Update the example and documentation to match implemented behavior. Record
   release executable size and runtime-linking details without claiming tiny-system
   suitability from a placeholder build.
8. Present the diff and results for review. Commit and push only when authorized.

## Scope boundary

This increment will not execute commands, resolve dependencies, alter environments,
change working directories, or introduce parallelism. Those need separate reviewed
increments. The full example in `examples/tatr.toml` remains illustrative until the
schema is agreed.

## Acceptance criteria

- A documented TOML file can be listed with predictable output.
- Invalid input fails clearly without starting a child process.
- Help and version work without a task file.
- Added dependencies and measured build costs are documented.
- The implementation and checks remain small and understandable.
