# Verification results

## Task working-directory increment (current)

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
