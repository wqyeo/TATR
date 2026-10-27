# Next increment proposal: local release packaging

Status: awaiting review. Do not implement until approved. The
[first-use increment](first-use-release-readiness.md) is complete; its
[release checklist](release-checklist.md) identifies packaging and support scope
as the next practical release decisions. No publication is authorized.

## Outcome

Prepare reviewable local archives from the tested installed distributions, verify
them after extraction, and state their runtime requirements accurately.

## Full proposed plan

1. Document the proposed package matrix: Windows x64/MSVC and Alpine Linux x64/musl
   using the tested toolchains. Label macOS, glibc Linux and other architectures
   unverified; make no broader compatibility or minimum-version guarantees.
2. Add an opt-in local packaging workflow using existing CMake/tools to build and
   install into isolated staging directories under build/. Include the executable,
   both MIT notices, a short package usage guide and an example TOML file. Add no
   runtime dependency or task-runner behavior change.
3. Produce clearly named Windows ZIP and Alpine Linux tar.gz archives, a manifest
   with version/platform/toolchain/runtime requirements, and SHA-256 checksums.
   Record contents and digests without claiming bit-for-bit reproducible builds.
4. Extract each archive into a fresh spaced path and verify its manifest, executable
   and license contents against staging. Reject missing or unexpected staged files;
   do not package build caches, private env files or logs.
5. Run the extracted executable's help/version/list/plan and a minimal task from
   another cwd on its native tested platform. Verify console/file logging and
   explicit dotenv loading. CMake may support smoke checks but is not a general
   runtime requirement. Record dependency and platform limitations.
6. Update README/checklist with local packaging commands and evidence; present
   archives and remaining release decisions for review. Commit/push only when
   authorized. Do not tag, upload/publish artifacts or activate remote CI/runners.

## Scope boundary

No release publication, registry/image upload, remote CI/runner changes,
cross-compilation, universal Linux claims, performance gates or new execution features.
