# glibc Linux compatibility increment

Status: approved and verified; the Debian 12 x64/glibc archive is included in the
0.2.0a release. The earlier 0.1.0a release remains unchanged. The plan below records
the original compatibility increment; see [packaging](packaging.md) for current
commands and [verification](verification.md) for its exact evidence.

## Full proposed plan

1. Select and document one pinned glibc Linux x64 build environment with a C++23
   compiler. Record distribution, libc, compiler, CMake and build-tool versions;
   state the proposed artifact's baseline without claiming universal Linux support.
2. Build a clean committed checkout and run the complete strict regression suite
   in that environment. Keep generated data under build/ and add no runtime features.
3. Exercise the native process backend through installed TATR: cwd/PATH lookup,
   dotenv/inline precedence, runtime arguments, console/file logs and failure exits.
   Investigate compatibility failures; bring material runtime fixes back for review.
4. Inspect the executable's shared dependencies and required glibc/C++ symbol
   versions. Record the actual runtime requirements and untested older systems.
5. Extend the opt-in packaging recipe for this tested baseline, reusing its manifest,
   allowlist, licenses, extraction/hash checks and smoke tests. Produce a local
   clearly labelled archive without modifying the existing 0.1.0a release assets.
6. Update the compatibility matrix, build/package instructions and verification
   evidence, then present the result for review. Commit/push and publication need
   authorization; do not publish another release or change CI/runners by default.

## Scope boundary

No cross-compilation, universal/static-Linux claims, new execution features,
performance gates, macOS claims without a native host, or automatic publication.
