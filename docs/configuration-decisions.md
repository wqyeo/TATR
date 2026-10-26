# Configuration increment: decision review

Status: dependency and schema approved; the configuration/listing increment is
implemented. See [the implemented contract](configuration.md) and
[verification results](verification.md).

This document records the first increment's decisions. The subsequent approved
dependency-planning increment adds `depends` and `plan TASK`; current behavior is
documented in the configuration contract rather than the historical schema below.

## Parser comparison

Measured on this Windows x64 machine with MSVC 19.51.36260.0, C++23,
`MinSizeRel`, and the static MSVC runtime (`/MT`). Each temporary executable parsed
`examples/tatr.toml` and printed alphabetically ordered task names and descriptions.
All three also returned exit code 2 for a missing file and malformed TOML.

| Candidate | Revision tested | Executable bytes | KiB |
| --- | --- | ---: | ---: |
| tomlc17 R261003 | `8d3766ddafa6515417f4885ae6878ef69fac401e` | 299,520 | 292.5 |
| toml++ v3.4.0 | `30172438cee64926dc41fdd9c11fb3ba5b2ba9de` | 318,976 | 311.5 |
| toml11 v4.4.0 | `be08ba2be2a964edcdb3d3e3ea8d100abc26f286` | 631,808 | 617.0 |

The existing placeholder release executable measured 214,016 bytes (209 KiB).
These measurements are whole-executable sizes for small experiments, not isolated
library sizes or predictions of the final runner's size. The programs use slightly
different library APIs to produce equivalent output. They are not comprehensive
benchmarks or exhaustive size tuning. toml++ formatters were disabled; toml11 used
its default value configuration, including comment/formatting support. Memory,
startup time, Linux, and macOS were not measured. Podman was unavailable.

Sources and integration considerations:

- [tomlc17](https://github.com/cktan/tomlc17/tree/8d3766ddafa6515417f4885ae6878ef69fac401e)
  is MIT licensed, advertises TOML 1.1 compliance, and offers a two-file C parser.
  The application would remain C++23; this dependency would add a C17 build target
  and need a small C++ ownership wrapper around its parse result.
- [toml++](https://github.com/marzer/tomlplusplus/tree/30172438cee64926dc41fdd9c11fb3ba5b2ba9de)
  is MIT licensed and supports TOML 1.0 with direct C++ integration. Its single
  header is easy to vendor; unused formatters can be disabled. It is a reasonable
  alternative if avoiding a C dependency is preferred.
- [toml11](https://github.com/ToruNiina/toml11/tree/be08ba2be2a964edcdb3d3e3ea8d100abc26f286)
  is MIT licensed and supports TOML 1.0 plus newer language features. Its comment
  and formatting preservation capabilities are beyond the current read-only need.
- [tomlc99](https://github.com/cktan/tomlc99) was excluded because its upstream
  marks it obsolete and points to tomlc17.

Recommendation: tomlc17 R261003, pinned to the full revision above. Vendor
`src/tomlc17.c`, `src/tomlc17.h`, its MIT license, and an upstream/version/checksum
note under `third_party/tomlc17/`. This permits offline builds from a checkout and
avoids a package manager or network download during configuration. Compile the C
file separately and hide its types behind a C++ configuration interface. Preserve
the third-party license with installed distributions.

Downloaded source checksums (SHA-256):

```text
tomlc17.c c382824bdfdd12f89a6a228d0838f99deae2f2c1763c3215547a0d7137ffa39d
tomlc17.h fa7f05a6057d7b4da1b63ce6185f07f5c734c98043a5d3a30ee7b9289060cba8
```

The evaluation sources and builds are temporary, under the ignored
`build/parser-evaluation/` directory. Following approval, the recommended parser
was integrated under `third_party/tomlc17/` without modifying its upstream files.

## Approved CLI and behavior

```text
tatr --help
tatr --version
tatr [--file PATH] list
```

- Default to `tatr.toml` in the current directory; no parent-directory discovery.
- Print usage when invoked without arguments.
- List tasks alphabetically, one per line: `name - description`, or just `name`
  when the description is absent or empty.
- Keep help and version independent of a task file.
- Return 0 on success and 2 for CLI or configuration errors. Send diagnostics to
  stderr with the file path and field, or source location where available.
- Reject unknown options, unexpected arguments, duplicate file options, and a
  missing argument after `--file`.

## Approved schema

```toml
version = 1

[tasks.build]
description = "Build the project"
```

- Require the integer `version = 1` and a `tasks` table; permit an empty tasks table
  so a newly created configuration can be listed successfully.
- Every named task must be a table. Its only supported field in this increment is
  an optional string `description`.
- Require task names to be nonempty and contain no control characters. Descriptions
  may be empty or Unicode, but must not contain control characters; listing remains
  one predictable line per task. Use TOML quoted keys for names containing dots.
- Reject unknown root/task fields and incorrect types. In particular, reject
  `exec`, `depends`, `cwd`, and `env` as unsupported for now.
- Configuration `version` describes TATR's schema; it is distinct from the TOML
  language version accepted by the parser.

`examples/tatr.toml` now uses this implemented schema. The fuller execution and
dependency example is retained in the design notes, explicitly marked as future
design. No task execution, dependency resolution, environment overrides, or working
directory changes are included.

## Approved implementation sequence (completed)

1. Integrate the pinned parser and license; keep dependency build settings separate
   from the application's warnings and language settings.
2. Add a minimal C++ configuration model, file loading, and strict schema validation.
3. Add help, version, file selection, and alphabetical listing in a separate CLI
   layer. Keep the entry point small.
4. Add focused parser/CLI checks for successful listing, empty tasks, ordering,
   absent descriptions, Unicode, malformed and missing files, duplicate TOML keys,
   unsupported versions/fields, invalid types, and malformed CLI arguments.
5. Build and check the Windows debug/release configurations. Try Linux through
   Podman if available; report Linux/macOS verification limits accurately.
6. Update documentation/examples and record the actual release size and runtime
   linkage. Present the diff and results for review before any further increment.
   Commit/push only when authorized for this increment.
