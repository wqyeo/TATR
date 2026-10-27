# Release 0.2.0a

Status: explicitly authorized by the user: commit/push accumulated approved work
and publish 0.2.0a. Preparation and all preflight checks passed. Publication uses final archives
rebuilt from the clean signed release commit, with remote download/hash checks.
No new runtime feature is part of this release increment.

## Approved release plan

1. Set version 0.2.0a and move accumulated changes into its changelog entry.
2. Refresh package runtime metadata and include public hook/SMTP examples.
3. Verify exact package contents, hashes, spaced-path extraction, help/version,
   task execution, dotenv/logging, completion hooks and read-only SMTP examples.
4. Exercise extracted Windows and Alpine release binaries against the existing
   disposable local SMTP/TLS fixtures. Verify the Debian 12 archive on its own
   baseline with strict regression and extracted command/hook checks. Record
   precise platform/client coverage without implying untested SMTP providers.
5. Create and verify an SSH-signed release commit with the configured identity.
6. Build final archives from that clean commit, add SHA256SUMS, create/verify
   the SSH-signed 0.2.0a tag and push main/tag. Never overwrite an existing tag.
7. Publish the GitLab alpha release with all three archives and checksums through
   the generic package registry, then download every asset and compare hashes.

Targets: Windows x64/MSVC ZIP; Alpine 3.21 x64/musl tar.gz; Debian 12 x64/glibc
2.36 tar.gz. macOS, other architectures and other glibc baselines remain unverified.
No CI activation, runner changes, real outbound mail or existing-release changes.

See [release notes](releases/0.2.0a.md), [packaging](packaging.md), and
[verification evidence](verification.md). Final checksums/source revision are in
the release artifacts and their manifests, avoiding a circular commit/hash record.
