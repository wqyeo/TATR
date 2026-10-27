# Changelog

Notable user-facing changes to TATR are recorded here.
The format follows [Keep a Changelog 2.0.0](https://keepachangelog.com/en/2.0.0/).
TATR currently uses alpha identifiers such as `0.1.0a`; this file preserves the
published naming without asserting a Semantic Versioning compatibility policy.

## [Unreleased]

## [0.2.0a] - 2026-10-27

### Added

- Structured SMTP tasks and completion-hook actions through external curl, with
  verified TLS, dotenv authentication, plain-text MIME messages, bounded input,
  sending timeouts, private stdin and scoped message-file cleanup.

- Run-completion command hooks for success, failure or always, with target filters,
  original-result context, explicit notification-failure policy and action records.

- Opt-in Node.js/PHP build, archive/move and database backup/restore workflow
  fixtures, with versioned verification evidence and action-log/failure checks.
- Debian 12 x64/glibc packages, verified with GCC 12.2/glibc 2.36, alongside
  Windows x64/MSVC and Alpine 3.21 x64/musl packages.

### Changed

- Shorter README explaining when TATR is useful and linking detailed workflow,
  build/install and file-operation guidance from the documentation index.
- Package manifests describe committed versus working-tree sources and the
  platform runtime baseline. Archives include a changelog and public hook/SMTP
  examples; Windows metadata includes native ACL/synchronization dependencies.

## [0.1.0a] - 2026-10-27

First experimental alpha.

### Added

- TOML-defined tasks with complete schema/dependency validation, task listing,
  dependency planning and sequential execution that stops on failure.
- Direct argument-array execution with task-specific working directories,
  child-only environment overrides, explicit dotenv loading and literal runtime
  arguments appended to the requested task.
- Console action records and optional append-only files with UTC filename tokens,
  level filtering, task outcomes and durations. Child output remains inherited.
- Windows x64/MSVC and Alpine Linux 3.21 x64/musl packages, each with a usage guide,
  public examples, file-hash/build manifest and both MIT license notices; archive
  checksums are provided alongside the downloads.

[Unreleased]: https://gitlab.com/wqyeo/thats-another-task-runner/-/compare/0.2.0a...main
[0.2.0a]: https://gitlab.com/wqyeo/thats-another-task-runner/-/releases/0.2.0a
[0.1.0a]: https://gitlab.com/wqyeo/thats-another-task-runner/-/releases/0.1.0a
