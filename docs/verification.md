# Verification results

## Alpha packaging (current)

Verified on 2026-10-27 for `0.1.0a`. CMake's numeric project version remains 0.1.0;
the display version and CLI version expectation now include the alpha suffix.

- Native Windows/MSVC strict build and **341 tests passed**.
- Alpine Linux 3.21 x64/GCC 14.2 strict MinSizeRel build and **332 tests passed**.
- Windows ZIP and Alpine/musl tar.gz packaging builds use strict MinSizeRel and
  isolated staging. Each includes exactly the executable, usage guide, three public
  example files, both MIT notices and MANIFEST.json.
- Both native recipes reject unexpected/missing staged/extracted files and compare
  every extracted file, including the manifest, against staging by SHA-256.
- Extracted help/version/list/plan and sample execution passed from outside each
  package, with a spaced extraction path. Version was `tatr 0.1.0a`; the CMake
  sample verified explicit dotenv loading, child output remaining outside action
  logs, and identical console/file records. Read-only commands created no logs.
- The manifest records the full source revision supplied for the final clean build,
  toolchain/generator/CMake versions, platform/runtime requirements and payload file
  hashes. Archive sidecars record SHA-256; final publication combines them into
  SHA256SUMS. There is no bit-for-bit reproducible-build claim.

Scripts/recipes and publication steps are in [packaging](packaging.md); release
notes are [0.1.0a](releases/0.1.0a.md). Local logs: build/package-windows.log,
build/package-linux.log, build/package-windows-strict.log and
build/package-linux-tests.log. Final clean-revision packaging logs use the suffix
`-final.log`; generated archives/staging/extractions remain under build/packages/.
No task execution code, new production dependency or remote CI setting changed.
Earlier Clang static-analysis/ASan/UBSan results remain baseline evidence; those
checks were not rerun solely for the version-string and packaging additions.
macOS, glibc Linux, MinGW, other architectures and older runtime versions remain
unverified. The Linux download is explicitly Alpine 3.21 x64/musl.

## First-use and release readiness

Verified on 2026-10-27 against runtime commit `01566dd`, using fresh local clones
under build/first-use/source and build/first-use/linux-source. Both clones remained
clean after their workflows (generated files are ignored beneath build/).
See the [release checklist](release-checklist.md) for readiness and open decisions.

- Windows x64 MSVC 19.51.36260.0 / CMake 4.3.2 / Visual Studio 18 2026: clean
  MinSizeRel build and chosen-prefix installation passed. The installed binary
  ran from another cwd and orchestrated the checked-in dev configure/build/test
  example with `-- --parallel 2`; **341 tests passed**. It ran its own installed
  copy, not the executable being rebuilt in the clone.
- Alpine Linux 3.21 x64 / GCC 14.2.0 / CMake 3.31.1 / Ninja (samurai 1.2): clean
  release build and installation passed. The installed binary orchestrated the
  same dev workflow from /tmp with **332 tests passed**. The source mount was
  read-only; only build outputs were writable.
- An installed Windows TATR orchestrated the opt-in container quality workflow,
  including strict Clang 19.1.4 build, format verification, static analysis and
  **332 tests passed**. No worker/service/remote pipeline was installed or enabled.
- Standalone C++23 sample projects on both platforms verified configure/build to
  build/output, installation to build/stage, and running the installed sample.
  Ordinary and spaced paths passed. The sample printed the expected inline-env
  override plus literal arguments with spaces/leading dashes, after explicit
  .env.example loading. Action log files were created relative to each config.
- The public file-operation example ran on both platforms with ordinary/spaced
  project paths. Hash/content comparisons matched LICENSE for copied/moved files;
  the original moved file was absent. All sample data remained under build/.
- Read-only commands and unknown targets created no log directory in fresh smoke
  fixtures. File-only child failure and missing-dotenv failure returned 1/2 and
  recorded the corresponding outcomes without console action records.
- Both installed MIT notices matched their sources. Runtime inspection confirmed
  Windows imports only KERNEL32.dll; Alpine needs libstdc++.so.6, libgcc_s.so.1 and
  libc.musl-x86_64.so.1. These are tested artifacts, not universal runtime claims.

The only setup failure was the Alpine image's missing Make for CMake's default
Unix Makefiles generator. Explicit Ninja selection succeeded after `--fresh`
cleared the failed generator cache. README now includes explicit dev/release Ninja
commands, direct installed-binary invocations, and PATH/mise guidance for examples.
No runtime changes or new automated tests were needed for these documentation fixes.

Evidence under build/first-use/ includes windows-clean-build.log,
windows-dogfood.log, installed-quality.log, linux-default-generator.log,
linux-clean-dogfood.log, windows-sample.log, windows-sample-spaces.log,
linux-samples-files.log, windows-files.log, windows-error-smoke.log,
linux-error-smoke.log, windows-dependencies.log and linux-dependencies.log.
Generated standalone sample sources and installed prefixes are retained there.
The earlier logging ASan/UBSan result remains the runtime sanitizer evidence.

macOS, glibc Linux, MinGW, older system/compiler versions and other architectures
remain unverified. No performance gates, runtime features, release publication,
remote CI or runner provisioning were introduced. Local archive packaging and
extraction verification are proposed separately for approval.

## Action logging increment

Verified on 2026-10-27 against the [approved logging plan](action-logging.md).

- Windows x64 MSVC 19.51 / CMake 4.3.2: strict Debug build and **341 tests passed**.
- Linux x64 Alpine 3.21 / Clang 19.1.4 / CMake 3.31.1: strict build, pinned
  clang-format and clang-tidy checks, and **332 tests passed**.
- Linux Clang ASan/UBSan: **334 tests passed**, including the two deliberate
  sanitizer-propagation checks; application, parser and helpers instrumented.
- Linux GCC 14.2.0: strict MinSizeRel build and **332 tests passed**.
- The 46 additional checks per platform cover default console records, file-only
  output, shared severity thresholds/off, child/native failure outcomes, dotenv
  errors, grouping/shared prerequisites, sensitive input exclusion, escaped task
  names, UTF-8/invalid bytes/controls, portable basenames, UTC token snapshots,
  append behavior, config-relative/absolute/spaced/Unicode paths and cwd independence.
- Read-only/off/invalid invocations are checked not to create log directories.
  Deterministic open failures use a regular file as a parent. Injected console/file
  streams verify failed writes and final flushes; runner injection verifies failed
  start records prevent launch and later failures prevent subsequent children.
  Linux also exercises a real file sink through a symlink to `/dev/full`, checking
  exit 2, direct stderr fallback with console=false, and no child side effects.
- Child stdout/stderr remain inherited and absent from action files. Windows
  unsigned 32-bit exit statuses and POSIX signal fields are verified. Help/version
  now check output writes and flushes, including failing injected output streams.
- Windows release build/install passed. The installed binary ran console+file and
  file-only examples from a config path with spaces and Unicode; console/file
  records matched, child stdout stayed separate, and both MIT notices matched.
  Linux GCC install/run also verified console/file equality from another cwd and
  both license files. Generated fixtures and records stay under build/.

The synchronous logger owns its file by scope, is noncopyable/nonmovable, takes an
explicit console reference and clock seam, and checks writes/flushes and explicit
close state. Runner errors preserve exit 0/1/2; log sink failures escape to the
top-level stderr fallback without recursive records. No production dependency,
worker thread, global logger, output capture, benchmark or CI activation was added.
Earlier process/config regression cases explicitly use level=off; the dedicated
logging suite verifies the new default and its stream separation.

Release binaries are 420,352 bytes on Windows (static MSVC runtime) and 157,832
bytes on Alpine GCC (shared runtime, unstripped). These are artifact observations,
not performance gates or tiny-device validation. Reproduction uses the existing
strict, quality, sanitizer and GCC container workflows. Logs are
build/logging-windows-build.log, logging-windows-tests.log, logging-quality-run.log,
logging-sanitizer-run.log and logging-gcc-run.log; installation evidence is in
logging-release-build.log, logging-linux-installed.log and logging-installed-*.log.

macOS, MinGW, other architectures/libcs, cross-process contention, permission/locked
file injection and genuine OS close-time failures remain unverified. Stream seams
exercise final flush failures; the production file owner checks close state too.
No locking, rotation/retention, fsync durability, child-output capture or changed
signal/cancellation behavior is claimed. External signals can still terminate the
runner. Logging and the earlier dotenv implementation are committed in `01566dd`.

## Explicit dotenv-file increment

Verified on 2026-10-27; see [the approved plan](dotenv-files.md) and
[dotenv contract](configuration.md#dotenv-files).

- Windows x64 MSVC 19.51.36260.0 / CMake 4.3.2: strict Debug build and all
  **295 tests passed**, including 70 new dotenv checks. MinSizeRel release
  compilation completed without warnings.
- Linux x64 Alpine 3.21 / Clang 19.1.4 / CMake 3.31.1: strict build, clang-format
  verification, and clang-tidy analysis (including dotenv.cpp) passed; all
  **286 tests passed**, including 68 new dotenv checks.
- The same Linux Clang toolchain passed **288 ASan/UBSan tests**, with the parser,
  application, test helper, and sanitizer-propagation probes instrumented.
- Linux x64 GCC 14.2.0: strict MinSizeRel build and all **286 tests passed**.
- Coverage adds byte-exact LF/CRLF/BOM/EOF fixtures, comments, optional export,
  literal values, empty values/files, quote/escape handling, UTF-8 values including
  a four-byte code point, invalid/overlong/surrogate/out-of-range/truncated UTF-8,
  NUL, malformed assignments/quotes, line diagnostics, duplicates and native
  casing, absolute/relative/Unicode/spaced file paths, cwd independence,
  prerequisite-generated files, precedence, PATH lookup, task isolation, read-only
  commands, no automatic discovery, and exact/over-limit 1 MiB inputs. Parse errors
  are checked not to print marked assignment contents.
- Windows verifies case-insensitive replacement across file/inline layers and
  rejection of drive-relative paths. POSIX accepts distinct case-variant names.
  Missing/non-file inputs fail after earlier prerequisite effects, as intended.
- Installation and installed Windows binary execution passed for inline precedence
  and Unicode file values. Both project and parser MIT notices remain installed.

The parser uses owned input/values and checked UTF-8 byte ranges; no production
dependency or process backend was added. File keys normalize through the existing
native name-key helper, and the runner merges into a temporary environment map
without modifying config or process-global environment. Child PATH lookup uses
the existing implementation. Empty files preserve the inherited environment.

| Release executable | Current bytes | Current KiB | Previous bytes | Change |
| --- | ---: | ---: | ---: | ---: |
| Windows x64, static MSVC runtime | 374,272 | 365.5 | 365,568 | +8,704 |
| Alpine Linux x64 GCC, shared runtime, unstripped | 126,648 | 123.7 | 115,992 | +10,656 |

Runtime dependencies remain KERNEL32.dll on Windows and libstdc++.so.6,
libgcc_s.so.1, and libc.musl-x86_64.so.1 on Alpine, verified using dumpbin/readelf.
Sizes exclude developer tooling/shared runtimes; no performance benchmark or
resource-budget enforcement was introduced.

Commands use the existing strict, quality, sanitizer, and GCC container workflows.
Logs beneath build/ are dotenv-windows-tests.log, dotenv-quality-run.log,
dotenv-sanitizer-run.log, and dotenv-gcc-run.log. The Windows focused dotenv log
is dotenv-windows-focused.log. Public examples/.env.example is explicitly selected
by the repository workflow; private .env files are not required or auto-loaded.
The Windows release executable ran that real configure/build/test workflow from
outside the repository root with runtime CTest arguments; all 295 checks passed.
The example log is build/dotenv-example-run.log.

macOS, MinGW, other architectures/libcs, permission-denial/locked-file injection,
and concurrent-file-edit races remain unverified. File reads are not atomic
snapshots; detected growth/truncation is rejected but edits are not serialized.
Existing cancellation/descendant-cleanup and total-memory limits remain unchanged.
The dotenv implementation is committed alongside logging in `01566dd`.

## MIT licensing and usage documentation

Verified on 2026-10-26. Added the project's MIT license (2026 Yeo Wen Qin), retained
tomlc17's notice, and included both files in the CMake installation. No runtime
behavior changed; the full-suite records below remain the runtime baseline.

- Windows release configure/build/install passed with the existing MSVC toolchain.
  Installed project/parser notices matched their source files by SHA-256.
- Linux GCC release configure/build/install passed in the pinned Alpine tool
  container. Both installed notices matched their sources byte-for-byte.
- The new `examples/file-operations.toml` ran successfully on Windows and Linux:
  source preparation, directory creation, copy, and same-volume rename completed.
  Copied/moved contents matched LICENSE and the original demo source was absent.
- The README's configure/build/install workflow ran on Windows in a generated
  project whose path contains spaces. Build output appeared under `build/output`
  and installation under `build/stage/bin`. Visual Studio selected Release via
  --config; CMAKE_BUILD_TYPE is a single-configuration generator setting.
- A copied Windows installation ran from a different directory, reporting its
  version and planning the file-operation example without needing its old prefix.

Local logs are `build/readme-destinations-check.log` and
`build/readme-linux-check.log`; generated projects, demo files, and relocated
installations remain under ignored build/. macOS and cross-volume move behavior
were not tested. CMake's rename operation is documented for a single volume;
other move semantics belong to the selected external tool.

The pending benchmark proposal was replaced with first-use/release readiness
after user feedback. No benchmark framework, release publication, or CI activation
was performed.

## Runtime task-argument increment (historical baseline)

Verified on 2026-10-26; see [the approved plan](runtime-arguments.md) and
[configuration contract](configuration.md).

- Windows x64 MSVC 19.51.36260.0 / CMake 4.3.2: strict Debug build and **225
  tests passed**. MinSizeRel release compilation passed without warnings.
- Linux x64 Alpine 3.21 / Clang 19.1.4 / CMake 3.31.1: strict build,
  clang-format verification, and clang-tidy analysis passed; **218 tests passed**.
- The same Clang toolchain passed **220 ASan/UBSan tests**, including the
  instrumented parser/application/child and sanitizer-propagation probes.
- Linux x64 GCC 14.2.0: strict MinSizeRel build and **218 tests passed**.
- End-to-end coverage adds configured/runtime argument order, no/empty separators,
  explicit empty strings, spaces, quotes, trailing backslashes, Unicode, embedded
  semicolons/control characters, literal shell metacharacters, child flags after
  the separator, config-file option placement, shared-prerequisite isolation,
  grouping rejection before side effects, cwd/env behavior, invalid syntax,
  unknown targets, invalid unrelated configuration, and child/launch failures.
- A Windows-specific fixture combines a 32,000-character configured argument with
  a 1,000-character runtime argument: each input alone fits, TATR can be launched,
  and the final child command correctly fails the 32,767-unit check with exit 2
  after its prerequisite. Existing native-size coverage remains intact.

Runtime argument spans borrow from the main/CLI-owned vector for the synchronous
run. The runner copies only the target command when appending nonempty arguments;
configuration and prerequisite commands remain const. No native process backend,
production dependency, shell policy, or cwd/env semantics changed.

| Release executable | Current bytes | Current KiB | Previous bytes | Change |
| --- | ---: | ---: | ---: | ---: |
| Windows x64, static MSVC runtime | 365,568 | 357.0 | 362,496 | +3,072 |
| Alpine Linux x64 GCC, shared runtime, unstripped | 115,992 | 113.3 | 110,728 | +5,264 |

Runtime dependencies remain `KERNEL32.dll` on Windows and `libstdc++.so.6`,
`libgcc_s.so.1`, and `libc.musl-x86_64.so.1` on Alpine, verified using dumpbin/readelf.
Sizes exclude shared runtimes and developer tooling. Command latency and total
memory budgets remain unmeasured; performance benchmarking is deferred.

Commands use the existing strict, quality, and sanitizer workflows and the same
GCC container flags recorded below. Logs under `build/` are
`arguments-windows-tests.log`, `arguments-quality-run.log`,
`arguments-sanitizer-run.log`, and `arguments-gcc-run.log`.
The Windows release binary ran the real example from outside the repository root
with `run check -- --parallel 2`; configure/build prerequisites succeeded without
receiving the CTest flags and all 225 checks passed. Installation and installed
binary argument forwarding, including an empty argument, also passed. The example
log is `arguments-example-run.log` under `build/`.

macOS, MinGW, other architectures/libcs, and a runtime-argument-specific POSIX
aggregate ARG_MAX case remain unverified. Existing POSIX environment-size failure
coverage still passes. Programs with custom Windows argument parsers retain their
own rules. Cancellation and descendant cleanup remain unmanaged.

## Task environment increment (historical baseline)

Verified on 2026-10-26; see [the approved plan](task-environment.md) and
[configuration contract](configuration.md).

- Windows x64 MSVC 19.51.36260.0 / CMake 4.3.2: strict Debug build and **195
  tests passed**. MinSizeRel release compilation passed without warnings.
- Linux x64 Alpine 3.21 / Clang 19.1.4 / CMake 3.31.1: strict build,
  clang-format verification, and clang-tidy analysis passed; **189 tests passed**.
- The same Linux Clang toolchain passed **191 ASan/UBSan tests**, including the
  instrumented parser, application, test child, and sanitizer-propagation probes.
- Linux x64 GCC 14.2.0: strict MinSizeRel build and **189 tests passed**.
- Coverage adds inherited/replaced/new/empty/Unicode/literal values, empty env
  tables, inherited nonportable Unicode names, isolation between tasks, PATH
  replacement independent of the parent's PATH, relative/empty/Unicode PATH entries
  with cwd and caller directories, preservation of lookup without PATH overrides,
  unset inherited PATH, read-only list/plan, invalid unrelated fields, grouping
  rejection, name/value validation, native size failures, and stopping subsequent
  tasks on launch/child errors. Invalid-value diagnostics are checked for leaks.
- Windows-specific cases cover case-insensitive replacement and PATH names plus
  rejection of conflicting case variants. POSIX accepts distinct case variants
  and checks permission-denied PATH candidates with and without a later match.
  No implicit interpreter is used for executable text on either platform.

The permission-denial fixture uses a directory as an executable candidate, which
reliably yields EACCES on Linux. The first fixture used a non-executable text file,
but the mounted Windows filesystem did not consistently enforce its execute bits
and instead returned ENOEXEC. This fixture correction retains the intended PATH
fallback assertion. POSIX environment assembly copies inherited entries once and
skips overridden names, avoiding repeated scans as overrides grow.

| Release executable | Current bytes | Current KiB | Previous bytes | Change |
| --- | ---: | ---: | ---: | ---: |
| Windows x64, static MSVC runtime | 362,496 | 354.0 | 350,208 | +12,288 |
| Alpine Linux x64 GCC, shared runtime, unstripped | 110,728 | 108.1 | 100,400 | +10,328 |

Runtime dependencies remain `KERNEL32.dll` on Windows and `libstdc++.so.6`,
`libgcc_s.so.1`, and `libc.musl-x86_64.so.1` on Alpine (dumpbin/readelf verified).
No production dependency was added. Sizes exclude developer tooling and shared
libraries; sanitizer runtimes are absent from normal release builds.

Commands follow the existing strict, quality, and sanitizer workflows; GCC uses
the same pinned container/toolchain flags recorded below. Logs under `build/` are
`env-windows-tests.log`, `env-quality-run.log`, `env-sanitizer-run.log`, and
`env-gcc-run.log`. The checked-in example now sets CTEST_OUTPUT_ON_FAILURE only on
its check task, exercising a real environment override. The Windows release
binary ran that configure/build/test workflow from outside the repository root;
all 195 checks passed. Installation and installed-binary execution with a Unicode
override also passed. The example log is `env-example-run.log` under `build/`.

macOS, MinGW, other architectures/libcs, unusual inherited duplicate Windows names,
and allocation/encoding failure injection remain unverified. Unicode override
values and inherited names are exercised, but no memory-safety proof or total
memory/startup budget is established. Native inherited drive entries are preserved
in code; separate injection of those hidden entries was not tested. Cancellation
and descendant cleanup remain unmanaged.

## Task working-directory increment (historical baseline)

Verified on 2026-10-26. See [the approved plan](task-working-directories.md)
and [the configuration contract](configuration.md).

- Windows x64 MSVC 19.51.36260.0 / CMake 4.3.2: strict build and **152 tests
  passed**. Clang-format verification, Linux clang-tidy analysis, and native
  Windows release compilation also passed.
- Linux x64 Alpine 3.21 / Clang 19.1.4 / CMake 3.31.1: strict quality build and
  **146 tests passed**. The instrumented parser/application/child build passed
  **148 ASan/UBSan tests**, including both sanitizer-propagation probes.
- Linux x64 GCC 14.2.0: strict MinSizeRel build and **146 tests passed**.
- Coverage adds relative/absolute/dot/Unicode/spaced working directories, relative
  --file, independent task directories, preservation of the caller's directory,
  prerequisite-created directories, missing/non-directory paths, executable and
  PATH lookup, read-only commands, unrelated missing directories, invalid fields,
  and stopping subsequent tasks on directory/launch/child failures. POSIX checks
  symlink/.. semantics; Windows checks rejection of ambiguous drive-relative forms.
- The old unsupported-cwd fixture was replaced by schema and runtime coverage.
  Windows fixture names avoid reserved device names; the NUL-value test uses
  `nul_path` as its directory name.

POSIX CMake probes select `posix_spawn_file_actions_addchdir_np` on the tested
Alpine libc; the standard name is preferred when available. The implementation
uses RAII spawn actions, never parent chdir. Windows uses a child directory plus
explicit executable resolution; its handle allowlist remains intact. No new
production dependency or remote CI configuration was introduced.

| Release executable | Current bytes | Current KiB | Previous bytes | Change |
| --- | ---: | ---: | ---: | ---: |
| Windows x64, static MSVC runtime | 350,208 | 342.0 | 334,848 | +15,360 |
| Alpine Linux x64 GCC, shared runtime, unstripped | 100,400 | 98.0 | 95,216 | +5,184 |

Runtime dependencies remain `KERNEL32.dll` on Windows and `libstdc++.so.6`,
`libgcc_s.so.1`, and `libc.musl-x86_64.so.1` on Alpine, verified using dumpbin and
readelf. Developer tooling and shared-library footprints are excluded.

Commands use the existing strict, quality, and sanitizer workflows. GCC uses the
same pinned tool image and flags recorded below. Logs under `build/` are
`cwd-windows-tests.log`, `cwd-quality-run.log`, `cwd-sanitizer-run.log`, and
`cwd-gcc-run.log`. The example workflow now explicitly selects the repository
directory through `cwd = ".."` in every executable task. The Windows release
binary ran that configure/build/test workflow successfully from `build/` rather
than the repository root; all 152 checks passed. Installation and installed-binary
execution with an explicit task directory also passed. The example log is
`cwd-example-run.log` under `build/`.

macOS, MinGW, other architectures/libcs, permission-denial cases, and concurrent
directory rename/removal remain unverified. Older libcs without either directory
spawn API fail configuration; their deployment compatibility is not established.
Directory checks are not race-proof, and this is not a filesystem sandbox.
Cancellation, descendant cleanup, memory budgets, and startup-time guarantees
remain outside the increment.

## C++ safety-hardening increment (historical baseline)

Verified on 2026-10-26. The approved scope is recorded in
[the completed plan](safety-hardening.md); commands and tool policy are in
[the development workflow](development.md).

- Windows x64 MSVC 19.51.36260.0 / SDK 10.0.26100.0 / CMake 4.3.2: strict Debug
  build passed with warnings treated as errors; all **122 tests passed**. New
  checks cover exact-limit and oversized configurations, exclusion of an unrelated
  inheritable event handle, absent standard handles, and stable process handle
  counts across 20 CreateProcess failures after native attribute setup. The
  failure fixture has a recognizable PE header with an invalid subsystem; it
  must reach CreateProcess failure rather than earlier executable rejection.
- Linux x64 Alpine 3.21 / Clang 19.1.4 / CMake 3.31.1: strict build, formatting,
  and clang-tidy analysis passed; all **118 tests passed**. Analysis covered all
  six production C++ translation units and the child helper. Standard-library
  header diagnostics were excluded by the configured project/header policy.
- Linux x64 with the same Clang toolchain: **120 ASan/UBSan tests passed**, with
  instrumentation on the C parser, project code, and test child. Two additional
  deliberate-defect tests verify actual ASan/UBSan diagnostics fail the shared
  CMake test harness even when nonzero application exits might be expected.
- Linux x64 GCC 14.2.0: strict MinSizeRel build and all **118 tests passed**.
- A temporary use-after-free probe made clang-tidy return a failing status with
  `clang-analyzer-cplusplus.NewDelete`, confirming enabled diagnostics are errors.
- Release build, installation, and installed Windows executable argument handling
  passed. No production dependency was added. A project C++ formatting baseline
  was established with clang-format 19.1.4; vendored files were excluded.

The first sanitizer run found undefined behavior in upstream tomlc17's
`page_create`: a null-pointer member expression used to calculate allocation
size. The local fix uses standard `offsetof` with the same allocation size.
[Parser provenance](../third_party/tomlc17/README.md) records the change and hashes.
No sanitizer check was disabled to accept it. Strict Clang also caught an implicit
signed-byte conversion in the test helper, now made explicit.

| Release executable | Current bytes | Current KiB | Previous bytes | Change |
| --- | ---: | ---: | ---: | ---: |
| Windows x64, static MSVC runtime | 334,848 | 327.0 | 333,824 | +1,024 |
| Alpine Linux x64 GCC, shared runtime, unstripped | 95,216 | 93.0 | 95,248 | -32 |

`dumpbin /DEPENDENTS` still reports only `KERNEL32.dll` on Windows. Linux
`readelf -d` still reports `libstdc++.so.6`, `libgcc_s.so.1`, and
`libc.musl-x86_64.so.1`. Sizes exclude shared runtimes and developer tools;
sanitizers are opt-in and absent from normal release builds.

Windows checks used `cmake --preset strict`, `cmake --build --preset strict`,
and `ctest --preset strict --parallel 4`. Container checks used the quality and
sanitize tasks documented in development.md. GCC used the pinned tool image with
`-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DTATR_STRICT=ON
-DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_TESTING=ON`. Local logs are under `build/`:
`windows-safety-tests.log`, `quality-run.log`, `sanitizer-run.log`,
`gcc-safety-run.log`, and `tidy-probe.log`.

No remote pipeline or runner was activated. macOS, MinGW, Windows sanitizer/static
analysis, other architectures/libcs, terminal cancellation, and descendant cleanup
remain unverified or unmanaged. The 1 MiB configuration limit caps input bytes;
it does not establish total memory or tiny-device budgets. Sanitizers and static
analysis do not prove absence of defects.

## Sequential command-execution increment (historical baseline)

Verified on 2026-10-26 using the toolchains below. No new production dependency
was added; the process implementation uses native Windows and POSIX APIs.

- Windows x64 MSVC Debug: all 117 CLI/process checks passed. Release builds
  completed without warnings. The release binary ran
  `--file examples/tatr.toml run check` end to end: configure, build, and all 117
  checks succeeded. Installation and installed-binary execution also passed.
- Linux x64 Alpine 3.21 / GCC 14.2.0 / CMake 3.31.1: all 116 checks passed in
  `MinSizeRel` through an ephemeral Podman container. A separate runtime-only
  container successfully executed the ordering fixture using the final binary.
- Tests retain configuration/planning coverage and add actual child-process
  argument reporting (empty strings, spaces, quotes, backslashes, Unicode,
  control characters, and shell metacharacters), inherited stdin/stdout/stderr,
  environment/cwd inheritance, PATH lookup, relative/Unicode executable paths,
  grouping, sequential shared prerequisites, nonzero status, spawn failure,
  missing targets, invalid configurations launching nothing, and read-only commands.
- Windows-specific checks verify a full 32-bit child exit status, the command-line
  length limit, and batch files requiring an explicit interpreter. The POSIX-only
  check verifies child termination by SIGTERM. Both platforms check explicit
  interpreter invocation and rejection of script text without an interpreter.
- Terminal Ctrl+C and signal delivery to TATR itself have not been exercised.
  No managed cancellation or descendant cleanup is provided. Windows programs
  with custom command-line parsers remain subject to their own argument rules.

| Release executable | Current bytes | Current KiB | Previous bytes | Change |
| --- | ---: | ---: | ---: | ---: |
| Windows x64, static MSVC runtime | 333,824 | 326.0 | 321,024 | +12,800 |
| Alpine Linux x64, shared runtime, unstripped | 95,248 | 93.0 | 89,120 | +6,128 |

Runtime dependencies remain `KERNEL32.dll` on Windows and `libstdc++.so.6`,
`libgcc_s.so.1`, and `libc.musl-x86_64.so.1` on Alpine Linux, verified using
`dumpbin /DEPENDENTS` and `readelf -d`. Shared runtime sizes are excluded.
macOS, MinGW, other architectures/libcs, memory usage, startup time, and tiny-device
suitability remain unverified.

Windows commands are the build/test/install commands below, plus
`build/release/MinSizeRel/tatr.exe --file examples/tatr.toml run check`.
Linux used the same read-only `/src` and writable `/out` mounts and build commands
below, with `ctest --test-dir /out --output-on-failure --parallel 2`.
The test helper is built only when `BUILD_TESTING` is enabled and is not installed.

## Dependency-planning increment (historical baseline)

Verified on 2026-10-26 with the same Windows and Linux toolchains recorded below.
This remains a read-only tool; no configured commands are executed.

- Windows x64 MSVC Debug: all 82 CLI checks passed via
  `ctest --preset dev --parallel 4`. Release build completed without warnings;
  listing, planning, and installation smoke checks passed.
- Linux x64 Alpine 3.21 / GCC 14.2.0 / CMake 3.31.1: all 82 checks passed in
  `MinSizeRel`, using an ephemeral Podman container with read-only source mounts.
- Coverage adds declared-order traversal, shared dependencies, forward references,
  unrelated tasks, Unicode names/paths, dotted names, CLI errors, dependency types,
  duplicates, missing references, self and multi-task cycles, disconnected cycles,
  and a 2,001-task chain. Failures must return 2 and produce no task output.
  Invalid unrelated components are rejected by both `list` and `plan`.
- Planning uses an explicit traversal stack, rather than native recursive calls.
  Shared prerequisites appear once; each requested target appears last.

| Release executable | Current bytes | Current KiB | Previous bytes | Change |
| --- | ---: | ---: | ---: | ---: |
| Windows x64, static MSVC runtime | 321,024 | 313.5 | 311,808 | +9,216 |
| Alpine Linux x64, shared runtime, unstripped | 89,120 | 87.0 | 74,296 | +14,824 |

Windows `dumpbin /DEPENDENTS` still reports only `KERNEL32.dll`. Linux `readelf -d`
still reports `libstdc++.so.6`, `libgcc_s.so.1`, and `libc.musl-x86_64.so.1`.
Executable sizes exclude Linux shared libraries and are not equivalent deployment
footprints. macOS and other architectures remain unverified; no startup-time,
memory-use, or tiny-device guarantees are established.

The Linux build/check commands below remain applicable; use
`/out/tatr --file /src/examples/tatr.toml plan check` for the new smoke check.
It prints `configure`, `build`, and `check`, one per line. On Windows the same
example passed using `build/release/MinSizeRel/tatr.exe`.

## Configuration/listing increment (historical baseline)

Verified on 2026-10-26. These results apply to this read-only increment, not a
complete task runner. All dependencies are built from the vendored checkout.

## Windows x64

- MSVC 19.51.36260.0, Visual Studio 18 2026, Windows SDK 10.0.26100.0.
- Development build: 51 CLI checks passed with `ctest --preset dev --parallel 4`.
- `MinSizeRel` release build: help/version/listing smoke checks and installation
  verified. The installed distribution includes the upstream parser license.
- Release executable: 311,808 bytes (304.5 KiB).
- `dumpbin /DEPENDENTS` reports only `KERNEL32.dll`. The MSVC runtime is linked
  statically; no separate VC runtime installation is needed for this binary.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --parallel 4
cmake --preset release
cmake --build --preset release
cmake --install build/release --config MinSizeRel --prefix build/install-check
```

## Linux x64

Verified in an ephemeral Podman container using the existing
`docker.io/library/alpine:3.21` image, GCC/G++ 14.2.0, CMake 3.31.1, and Ninja
(Alpine's samurai implementation). Source was mounted read-only at `/src`, with a
writable build directory at `/out`.

```sh
apk add --no-cache g++ cmake ninja file binutils
cmake -S /src -B /out -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DBUILD_TESTING=ON
cmake --build /out --parallel 2
ctest --test-dir /out --output-on-failure --parallel 2
/out/tatr --file /src/examples/tatr.toml list
stat -c '%s bytes' /out/tatr
file /out/tatr
readelf -d /out/tatr
```

- All 51 CLI checks passed in the optimized build.
- Unstripped executable: 74,296 bytes (72.6 KiB).
- Dynamically linked to `libstdc++.so.6`, `libgcc_s.so.1`, and
  `libc.musl-x86_64.so.1`. This Alpine binary requires a compatible musl environment;
  it is not a generic Linux distribution artifact.

## Coverage and limits

The CLI checks exercise actual executable output, exit status, and stderr for
listing, ordering, empty configurations, Unicode names/paths, quoted dotted names,
missing/malformed files, duplicate keys, unsupported schema versions and fields,
wrong types, control characters, invalid CLI arguments, and lack of parent-file
discovery. They also verify that validation failures emit no partial listing.

macOS, other architectures, MinGW, and other Linux libc/toolchain combinations have
not been verified. This increment does not establish startup-time, memory, static
Linux linking, or tiny-device support guarantees. Whole-executable sizes exclude
shared runtime libraries and should not be compared as equivalent deployment
footprints across platforms. No size budget has been adopted yet.
