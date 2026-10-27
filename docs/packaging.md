# Packaging TATR alpha releases

The user authorized publishing 0.2.0a with Windows, Alpine/musl and Debian/glibc
x64 packages. See [the release plan](release-0.2.0a.md). Existing 0.1.0a assets and
its signed tag are retained unchanged. The tag and CLI use the exact alpha name;
CMake's numeric project version is 0.2.0 and TATR_VERSION appends a.

## Verified targets

| Package | Runtime baseline |
| --- | --- |
| Windows x64/MSVC ZIP | Static MSVC runtime; Windows system ACL/synchronization APIs (KERNEL32, ADVAPI32 and api-ms-win-core-synch-l1-2-0). |
| Alpine 3.21 x64/musl tar.gz | libstdc++.so.6, libgcc_s.so.1, libc.musl-x86_64.so.1. |
| Debian 12 x64/glibc tar.gz | glibc 2.36 build baseline, libstdc++.so.6, libgcc_s.so.1, libc.so.6; ELF interpreter /lib64/ld-linux-x86-64.so.2. |

These are tested environments, not minimum OS/compiler guarantees. macOS, other
architectures, older systems and other glibc baselines remain unverified. The
recipe rejects unsupported OS/compiler/architecture combinations. It does not
cross-compile or produce a universal Linux executable. SMTP additionally needs
an external native SMTP/TLS-capable curl client; it is not bundled. Core builds
need no SMTP/TLS library downloads. See [SMTP requirements](smtp.md).

## Build verified archives

Use a clean committed checkout. Build tools and Git are packaging prerequisites,
not runtime dependencies. Windows PowerShell:

```powershell
$revision = git rev-parse HEAD
mise exec -- cmake "-DSOURCE_REVISION=$revision" -P tools/package.cmake
```

Alpine on this workstation, using the pinned quality image:

```powershell
New-Item -ItemType Directory -Force build/packages | Out-Null
$revision = git rev-parse HEAD
podman run --rm -v "${PWD}:/src:ro" -v "${PWD}/build/packages:/src/build/packages:rw" `
  localhost/tatr-quality:llvm19.1.4 cmake "-DSOURCE_REVISION=$revision" -P tools/package.cmake
```

Debian/glibc:

```powershell
podman build -t localhost/tatr-glibc:debian12-gcc12 -f tools/Containerfile.glibc tools
$revision = git rev-parse HEAD
podman run --rm -v "${PWD}:/src:ro" -v "${PWD}/build/packages:/src/build/packages:rw" `
  localhost/tatr-glibc:debian12-gcc12 cmake "-DSOURCE_REVISION=$revision" -P tools/package.cmake
```

Image setup is described in [development checks](development.md). Pass
-DSOURCE_STATE=working-tree only for local preflight packages, never a final
release. The caller supplies provenance; the recipe does not audit Git from the
container. After committing, rebuild final archives from the clean new revision
with SOURCE_STATE=committed (the default).

Output is under build/packages/PLATFORM: tatr-0.2.0a-PLATFORM.zip or .tar.gz,
a matching .sha256, a reusable build directory, and unique staging/extraction
runs. Existing release archives with other names are not overwritten. Fresh
run trees avoid recursive cleanup and stale payloads.

The recipe builds strict MinSizeRel, installs into fresh staging, adds only the
allowlisted payload below and creates MANIFEST.json. It checks the complete file
list, archives, extracts into a fresh spaced path, compares each file hash, and
runs extracted help/version/list/plan/sample execution from another directory.
The sample verifies dotenv and matching console/file action records. Additional
checks run successful/failed completion hooks and inspect the SMTP example without
credentials, curl lookup or network. Failure of any check prevents success.
Archives are not promised byte-for-byte reproducible across builds.

```text
bin/tatr[.exe]
README.md
CHANGELOG.md
MANIFEST.json
examples/tatr.toml
examples/hello.cmake
examples/.env.example
examples/completion-hooks.toml
examples/hook-summary.cmake
examples/smtp.toml
examples/.env.smtp.example
share/tatr/licenses/tatr-LICENSE
share/tatr/licenses/tomlc17-LICENSE
```

The manifest records version, platform, compiler, generator, CMake, runtime,
source revision/state and SHA-256 for the twelve payload files. The archive
checksum covers the manifest too. No private dotenv, generated logs or caches
are shipped. Public dotenv examples contain placeholders only. CMake 3.25+ is
needed for the demonstration commands, not ordinary TATR or SMTP execution.

## Publication

1. Complete strict regression, packaged smoke and scoped SMTP/TLS verification.
2. SSH-sign the source/docs commit; build final packages from that clean revision
   and combine sidecars into SHA256SUMS without changing the committed source.
3. Create and verify the signed 0.2.0a tag at that commit, then push main and tag.
   Do not overwrite existing tags/releases.
4. Publish with docs/releases/0.2.0a.md and the three archives plus SHA256SUMS,
   using the project's generic package registry for download links.
5. Download the published assets, compare hashes and verify the remote tag/release
   resolves to the expected source commit.

Publication is authorized for this alpha. Later releases need their own approval.
No CI, runner or catalog configuration is changed by this procedure.
