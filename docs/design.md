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
- MIT project license; retain the vendored parser's separate MIT notice.
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

These boundaries guide reviewed increments; current capabilities are listed below.

## Current implementation

TATR loads TOML, validates a versioned schema and dependency graph, lists tasks,
displays prerequisite order with `plan`, and executes argument arrays sequentially
with `run`. See [the configuration contract](configuration.md). A pinned tomlc17
dependency provides TOML parsing; native OS APIs provide process execution.

## Supported format

Run this example from the repository root:

```toml
version = 1

[tasks.configure]
exec = ["cmake", "--preset", "dev"]

[tasks.build]
depends = ["configure"]
exec = ["cmake", "--build", "--preset", "dev"]
```

## Increment sequence

1. Parse and validate a versioned task file; list task names and descriptions
   (implemented).
2. Resolve dependencies and display task order without executing commands
   (implemented).
3. Execute tasks sequentially, stopping on failure and reporting original child
   statuses (implemented; runner exits 1 for child failures, 2 for its own errors).
4. Establish local strict builds, static analysis, sanitizers, bounded config
   input, and restricted Windows handle inheritance (implemented; see
   [development checks](development.md)).
5. Add task working directories resolved from the configuration file (implemented).
6. Add task environment overrides with explicit inheritance and lookup semantics
   (implemented).
7. Append literal runtime arguments to the requested task (implemented).
8. Dogfood the existing workflows and establish first-use/release readiness
   (proposed). Performance benchmarking is deferred.

Each increment needs review before implementation. Tests should cover meaningful
behavior as it is introduced. Parallel execution, caching, remote execution,
plugins, and CI platform integration are deferred.

## Decisions still open

- Future TOML schema and CLI extensions beyond basic sequential execution.
- Parser upgrade policy beyond the approved pinned, vendored tomlc17 dependency.
- Supported compiler versions, operating-system versions, and architectures.
- Numeric budgets for executable size, startup time, and memory use.
- Static versus dynamic runtime linking and release packaging by platform.
- Process cancellation, signal handling, and shell behavior on each platform.

The scaffold uses a static MSVC runtime and a `MinSizeRel` release preset as
starting build choices. They do not establish a release-size guarantee or an
equivalent linking policy for other platforms.
