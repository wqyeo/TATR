# Development checks

Use value types and focused functions; introduce classes when they own a resource
or protect an invariant. Prefer owning standard containers, RAII, const references,
and explicit lifetimes. Keep raw native handles and pointer conversions inside the
platform boundary. Avoid adding virtual interfaces without a concrete need.

## Local workflow

1. Edit the approved scope, adding regression checks for meaningful behavior.
2. Run `mise run format` and inspect the diff.
3. Run `mise run strict` for the native compiler build and regression suite.
4. Run `mise run quality` for Linux Clang strict compilation, format verification,
   focused clang-tidy analysis, and regression tests.
5. Run `mise run sanitize` for Linux ASan/UBSan tests, including the TOML parser.

Run `mise run format-check` or `mise run analyze` independently when useful. No
Git hook is installed; local tasks explicitly return a failure when a gate fails.
Normal dev/release builds retain their existing warning policy and carry no
sanitizer instrumentation. None of these tools is a production dependency.

## Changelog maintenance

Record notable user-facing changes in [CHANGELOG.md](../CHANGELOG.md) under
Unreleased, using the applicable Added, Changed, Deprecated, Removed, Fixed or
Security category. Describe the behavior and its effect on users; omit empty
categories and routine internal edits. Keep the newest release first and use
YYYY-MM-DD dates verified against the actual release.

At release time, move the appropriate Unreleased entries into the new version,
update its GitLab comparison/release links and leave a fresh Unreleased section.
Use that section as the starting point for the release announcement, adding
installation or migration detail where useful. Preserve existing identifiers
such as 0.1.0a; changing the versioning policy needs separate review. Local
compatibility candidates belong under Unreleased until actually published.
This is a manual convention based on [Keep a Changelog 2.0.0](https://keepachangelog.com/en/2.0.0/),
with no required hook, generator or remote pipeline.

`mise run strict` is equivalent to:

```sh
cmake --preset strict
cmake --build --preset strict
ctest --preset strict --parallel 4
```

The container tasks need Podman and a running Podman machine on Windows/macOS.
They invoke `cmake -DMODE=<task> -P tools/container.cmake`, which builds a cached
local image from `tools/Containerfile`. The first build needs network access and
roughly 745 MiB of installed container packages, excluding image-layer overhead.
Later checks reuse the image. Ephemeral containers are removed on exit; the image
and build outputs remain for reuse. No service or runner is installed.

The image pins Alpine's base digest, LLVM/clang-tidy/clang-format 19.1.4, compiler-rt
19.1.4, GCC 14.2.0, CMake 3.31.1, and samurai 1.2 package revisions. Transitive
packages still resolve from Alpine 3.21 repositories; this is a tool-version pin,
not a complete historical package mirror. Updating tools should be a reviewed
change, including rerunning the gates and checking analyzer/style changes.

Sources are mounted read-only except for the explicit formatting command. Output
mounts are `build/quality-linux` and `build/sanitizer-linux`. Avoid simultaneous
commands that share the same output directory. Formatting must complete before
builds or analysis start. Parallel build/test jobs are limited to two in containers.

## glibc compatibility image

The optional glibc compatibility image is separate from the LLVM/Alpine quality
image: `podman build -t localhost/tatr-glibc:debian12-gcc12 -f tools/Containerfile.glibc tools`.
It pins an amd64 Debian 12 base digest, GCC 12.2, CMake 3.25.1, Ninja 1.11.1,
binutils 2.40, glibc 2.36 and libstdc++ package versions. Transitive APT packages
still resolve from Debian repositories; this is not a complete snapshot mirror.
See [glibc verification](verification.md#glibc-linux-compatibility) for the
clean-build/test commands and runtime limitations. No GCC sanitizer or Clang tool
claim is made for this image; the existing Alpine quality image retains that role.

## Enforced policy and exceptions

- Strict MSVC uses `/WX` and selected narrowing/shadowing diagnostics in addition
  to `/W4 /permissive-`. GCC/Clang use `-Werror -Wconversion -Wsign-conversion
  -Wshadow` in addition to existing warning flags. Vendored C is outside this
  project warning policy.
- `.clang-tidy` enables core/C++/dead-code analyzer checks and a small explicit
  selection for dangling handles, moved objects, sizeof/memory mistakes, and
  narrowing conversions. Enabled project diagnostics fail the command. Compilation
  databases select actual platform translation units; Linux analysis does not
  inspect Windows-only code. System-header diagnostics are excluded.
- Ownership is explicit at the native API boundary. POSIX `posix_spawnp` takes
  mutable argv pointers even though the argument strings are not modified; the
  localized `const_cast` adapts this legacy signature. Windows opaque attribute
  storage uses aligned `operator new`, paired with `operator delete`, while a
  separate RAII deleter destroys initialized attributes before storage is freed.
  No global cast/ownership-rule suppressions are added.
- Formatting covers project C++ in `src/` and `tests/`, including Windows-only
  files. It excludes `third_party/`; upstream style and provenance are retained.
- Sanitizers instrument project C++, the C parser, and the test child. Recovery is
  disabled. The test harness rejects sanitizer diagnostics independently of the
  expected application status. Two deliberate-defect self-tests prove ASan/UBSan
  findings fail that harness; their source is built only in sanitizer mode and
  is excluded from normal static-analysis builds.

The parser has a documented local `offsetof` fix discovered by UBSan; see
[provenance](../third_party/tomlc17/README.md). Do not suppress this diagnostic or
exclude the parser from instrumentation to make checks pass.

These checks detect classes of defects. They do not prove memory safety, resource
bounds, or SOLID compliance. Native Windows tests exercise its execution path;
Windows static analysis and MSVC ASan remain follow-ups. macOS execution is not
verified. Fuzzing is a useful later addition after these gates are stable.

## GitLab CI proposal (not activated)

Keep the CI commands the same as local checks. A first pipeline can run two Linux
jobs: `quality` and `sanitize`. Use a container-capable runner and a reviewed image
built from `tools/Containerfile`, published and pinned by digest. The following is
a proposal for a future `.gitlab-ci.yml`; the image reference is a placeholder:

```yaml
stages: [check]
default:
  image: registry.gitlab.com/wqyeo/thats-another-task-runner/quality@sha256:REVIEWED_DIGEST
  before_script:
    - mkdir -p /out
quality:
  stage: check
  script:
    - cmake -DMODE=quality -P "$CI_PROJECT_DIR/tools/linux-checks.cmake"
sanitize:
  stage: check
  script:
    - cmake -DMODE=sanitize -P "$CI_PROJECT_DIR/tools/linux-checks.cmake"
```

The script discovers sources from its own location; the job needs writable `/out`.
Do not set `allow_failure` on quality gates. Once a verified
pipeline exists, require successful pipelines before merging. Add native Windows
MSVC and macOS AppleClang jobs when matching runners are available; a Linux build
does not verify their native process APIs. Hosted GitLab runners consume namespace
compute minutes; inspect availability before enabling jobs. No remote settings,
pipeline configuration, image publication, or runner provisioning changed here.

## References

- [clang-tidy](https://clang.llvm.org/extra/clang-tidy/)
- [clang-format](https://clang.llvm.org/docs/ClangFormat.html)
- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [GitLab hosted runners](https://docs.gitlab.com/ci/runners/hosted_runners/)
