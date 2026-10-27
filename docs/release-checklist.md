# First-release checklist

First-use evidence recorded on 2026-10-27 against runtime commit `01566dd`.
The application remains pre-release, built from source. No release/tag, downloadable
package, remote pipeline, or runner was published/configured by this increment.

## Verified locally

- [x] Fresh local clones build without downloading production dependencies.
  The TOML parser is vendored; obtaining the compiler/build tools is separate.
- [x] Chosen-prefix installation works with spaces in the prefix on Windows/Linux.
  The installed executable runs outside the checkout without rebuilding itself.
- [x] Installed TATR runs the checked-in configure/build/test example, including
  explicit `.env.example`, config-relative cwd and runtime CTest arguments.
- [x] An installed Windows binary orchestrates the opt-in Podman quality workflow:
  strict Clang build, format verification, static analysis and Linux regression tests.
- [x] Sample C++23 projects configure/build/install into declared directories on
  both platforms, with ordinary and spaced project paths. A launched installed
  sample verifies dotenv/inline precedence and literal runtime arguments.
- [x] The checked-in copy/move example runs with ordinary and spaced project paths.
  Copied/moved contents match LICENSE and the moved source is absent afterward.
- [x] File logging records TATR actions separately from child output. Read-only
  commands and unknown targets create no log directory. File-only child and dotenv
  failures retain exit 1/2 and record their outcomes without console action records.
- [x] Both installed MIT notices match the source notices by hash/content comparison.
- [x] Runtime imports/dependencies inspected with dumpbin/readelf.

| Tested environment | Build tools | First-use result |
| --- | --- | --- |
| Windows x64 | Visual Studio 18 2026, MSVC 19.51.36260.0, CMake 4.3.2 | Release install, 341 dev tests, samples and file operations passed |
| Alpine Linux 3.21 x64 / musl | GCC 14.2.0, CMake 3.31.1, Ninja through samurai 1.2 | Release install, 332 dev tests, samples and file operations passed |
| Alpine Linux 3.21 x64 / musl, quality image | Clang/clang-tidy/clang-format 19.1.4 | Installed-runner quality workflow and 332 tests passed |

These are tested versions, not guaranteed minimum compiler/OS versions. The earlier
logging increment also passed 334 Linux ASan/UBSan checks. This documentation-only
increment does not change runtime code or add a test framework/performance gate.

## Package contents and runtime requirements

The current CMake install provides:

```text
bin/tatr                 # tatr.exe on Windows
share/tatr/licenses/tatr-LICENSE
share/tatr/licenses/tomlc17-LICENSE
```

Keep both notices when redistributing. Configuration files, scripts and tools
referenced by tasks must be supplied separately. TATR does not require CMake at
runtime for arbitrary workflows; the checked-in demonstration workflows do.

The tested Windows executable uses the static MSVC runtime and imports only
KERNEL32.dll. The tested Alpine executable needs libstdc++.so.6, libgcc_s.so.1 and
libc.musl-x86_64.so.1. It is not a general Linux/glibc package. Neither observation
establishes compatibility with older systems or another architecture.

## First-use friction and corrections

1. The Alpine tool image contains Ninja but no Make. The default release configure
   selected Unix Makefiles and failed before compilation. Explicit `-G Ninja`
   succeeded; README now gives this option for dev and release. A failed cache
   from another generator needs `--fresh` or a different build directory.
2. Installing a binary does not add its bin directory or the example's build tools
   to PATH. README now shows direct installed-binary invocations, the optional
   `mise exec --` wrapper, and Ninja selection for the configure task.
3. Build output locations and install roots belong to the selected build tool.
   The README's configure/build/install example was exercised without runtime
   changes. Do not move a CMake build tree to deploy it; install first.

No task-runner behavior defect was identified in these workflows. The next
practical increment proposed for review is local release packaging and extraction
checks. Native macOS compatibility requires a macOS host and remains separate.

## Before publishing any first release

- [ ] Approve a release version/channel and platform/toolchain support statement.
  Version output is currently 0.1.0; that does not establish a published release.
- [ ] Produce clearly named local platform archives with both MIT notices,
  installation/usage instructions and checksums; verify after extraction.
- [ ] Decide whether the first Linux artifact intentionally targets Alpine/musl
  or needs additional glibc builds/tests. Do not call an Alpine binary universal.
- [ ] Verify native macOS before distributing/claiming a supported macOS artifact.
  Alternatively, scope the first release explicitly to the tested Windows/Linux
  environments and retain macOS as an unverified target.
- [ ] Record unsupported/untested architectures, MinGW and older runtime versions.
- [ ] Review known execution limits: no timeout/cancellation/process-tree cleanup,
  parallel execution, rotation/retention or child-output capture. Existing POSIX
  descriptor inheritance and concurrent-file limitations remain documented.
- [ ] Obtain explicit authorization before tagging/publishing artifacts or changing
  remote CI/runner settings. Local readiness checks do not authorize those actions.

Generated sample sources, installed trees and evidence remain under ignored
`build/first-use/`. See [verification](verification.md) for log names and
[README build/install instructions](../README.md#build) for the supported commands.
