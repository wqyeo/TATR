# Design notes

## Problem

Not every organization or infrastructure environment can afford to provision and
maintain CI/CD infrastructure and runners. Work still needs to be repeatable,
whether it runs on a developer machine, a server, or a small device.

A large shell script is often the practical starting point. As it grows, task
selection, ordering, environment setup, and error handling become hard to follow.
TATR will put that orchestration into a readable TOML file.

## Agreed direction

- C++23 implementation targeting Linux, macOS, and Windows.
- Lean, minimal, and small enough for small or tiny systems.
- TOML describes tasks and their actions.
- Useful without CI/CD infrastructure or dedicated runners.

## Proposed boundaries

- One executable and a local task file for the basic workflow.
- No daemon, server, database, or account required.
- TOML describes orchestration; scripts and programs contain procedural logic.
- Explicit argument arrays for direct execution. Shell commands name their shell
  explicitly, rather than relying on an implicit cross-platform shell.
- Deterministic dependency ordering, understandable output, and clear failures.
- A portable runner does not make platform-specific commands portable.
- Add dependencies and features only when their benefit justifies their cost.

These boundaries guide future proposals; they are not implemented features.

## Possible first capabilities

1. Parse and validate a versioned task file; list task names and descriptions.
2. Resolve dependencies and display a dry run without executing commands.
3. Execute tasks sequentially, stopping on failure and preserving useful exit codes.
4. Add task working directories and environment overrides with explicit semantics.

Each increment needs review before implementation. Tests should cover meaningful
behavior as it is introduced. Parallel execution, caching, remote execution,
plugins, and CI platform integration are deferred.

## Decisions still open

- Exact TOML schema and CLI syntax, including configuration discovery.
- TOML parser choice, license, dependency delivery, and binary-size cost.
- Supported compiler versions, operating-system versions, and architectures.
- Numeric budgets for executable size, startup time, and memory use.
- Static versus dynamic runtime linking and release packaging by platform.
- Process cancellation, signal handling, and shell behavior on each platform.
- Project license.

The scaffold uses a static MSVC runtime and a `MinSizeRel` release preset as
starting build choices. They do not establish a release-size guarantee or an
equivalent linking policy for other platforms.
