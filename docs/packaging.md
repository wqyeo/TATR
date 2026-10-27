# Packaging 0.1.0a

The user approved the local packaging plan and explicitly authorized publishing
`0.1.0a` to GitLab. The tag and executable version use that exact alpha identifier;
CMake's numeric project version remains 0.1.0 because its VERSION field is numeric.
No task execution behavior or production dependency is changed.

## Supported recipes

- Windows x64, native MSVC, ZIP; static MSVC runtime, KERNEL32.dll import.
- Alpine Linux 3.21 x64, native GCC and Ninja, tar.gz; requires libstdc++.so.6,
  libgcc_s.so.1 and libc.musl-x86_64.so.1.

These match the tested environments, not minimum OS/compiler guarantees. macOS,
glibc Linux, MinGW, other architectures and older systems remain unverified.
The script rejects a different OS/architecture/compiler family; it does not
cross-compile or build a universal Linux binary. Installed dependencies still
need to be available on the destination system.

## Local commands

Use a clean committed checkout. Build tools and Git are packaging prerequisites;
they are not dependencies of the distributed task runner. On Windows PowerShell:

```powershell
$revision = git rev-parse HEAD
mise exec -- cmake "-DSOURCE_REVISION=$revision" -P tools/package.cmake
```

For Alpine on this Windows workstation, with the existing pinned tool image:

```powershell
New-Item -ItemType Directory -Force build/packages | Out-Null
$revision = git rev-parse HEAD
podman run --rm -v "${PWD}:/src:ro" -v "${PWD}/build/packages:/src/build/packages:rw" `
  localhost/tatr-quality:llvm19.1.4 cmake "-DSOURCE_REVISION=$revision" -P tools/package.cmake
```

Build/reuse the optional image using tools/Containerfile as described in
[development checks](development.md). The image's first build needs network access.
Inside a native Alpine 3.21 checkout with the prerequisites installed, the same
`cmake -DSOURCE_REVISION=<full-commit-SHA> -P tools/package.cmake` command works.
The caller supplies the full SHA of the clean source being packaged; the manifest
does not independently establish source provenance from a read-only container.

Output is under build/packages/<platform>:

- tatr-0.1.0a-windows-x86_64-msvc.zip or
  tatr-0.1.0a-linux-x86_64-alpine3.21-musl.tar.gz.
- A matching .sha256 file. Release publication combines these into SHA256SUMS.
- A reusable build directory and unique runs/<timestamp>-<nonce> staging/extraction
  trees. Archives/checksum sidecars are replaced on reruns; run trees are retained.

The recipe builds MinSizeRel with strict diagnostics, installs into fresh staging,
adds exactly the seven payload files below, and writes MANIFEST.json. It checks
the complete staging list before archiving, extracts into a fresh spaced path,
compares every file including the manifest against staging, and runs the extracted
help/version/list/plan/sample task from another cwd. The sample checks explicit
dotenv loading and matching console/file records, without capturing child output.
The script reports success only after these checks pass. It does not promise
bit-for-bit reproducible archives or delete old run trees recursively.

```text
bin/tatr[.exe]
README.md
MANIFEST.json
examples/tatr.toml
examples/hello.cmake
examples/.env.example
share/tatr/licenses/tatr-LICENSE
share/tatr/licenses/tomlc17-LICENSE
```

The manifest records version, platform, compiler, generator, CMake version, runtime
requirements, supplied source revision and SHA-256 hashes of the seven payload
files. The manifest itself is covered by the archive checksum. No build caches,
private dotenv files or generated logs are packaged. The public .env.example is
intentional. CMake 3.25+ is needed only to run the included demonstration task.

## Publication procedure

1. Complete native strict tests and package extraction/smoke verification.
2. Commit the source/docs with SSH signing, then build the final archives from
   that clean revision and combine their sidecars into SHA256SUMS.
3. Create and verify the SSH-signed `0.1.0a` tag at that revision; push the branch
   and that tag. Do not overwrite an existing tag/release.
4. Create the GitLab alpha release using docs/releases/0.1.0a.md as the notes file;
   attach the two archives and SHA256SUMS through the generic package registry.
5. Download the published assets and compare hashes against the verified local
   files. Confirm the remote tag/release points at the expected commit.

Publication is explicitly authorized for this alpha only. Future releases require
their own approval. No remote CI, runner or catalog configuration is changed.
