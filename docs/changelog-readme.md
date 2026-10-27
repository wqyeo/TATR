# Changelog and lean README increment

Status: approved and implemented; documentation verification is recorded in
verification.md. Requested while the approved glibc increment was in progress.

## Outcome

A concise front door explaining why/when to use TATR, with detailed guidance in
docs/ and a curated changelog based on https://keepachangelog.com/en/2.0.0/.

## Approved plan

1. Inventory the README and existing documentation. Keep the introduction, intended
   use, one copyable quickstart, release/platform status, minimal build/install
   guidance, documentation links and license. Map longer configuration, logging,
   dotenv, file-operation, developer and project-layout material to appropriate
   docs/ pages before removing duplication. Preserve useful anchors or update links.
2. Add CHANGELOG.md with a short preamble linking Keep a Changelog 2.0.0 and an
   honest description of the current alpha naming. Preserve the existing 0.1.0a
   identifier; do not assert a new SemVer policy or rename tags. Use Unreleased
   first, then a dated 0.1.0a entry based on the verified tag/release history, newest
   first. Use only applicable Added/Changed/Deprecated/Removed/Fixed/Security
   groups, and write notable user-facing changes rather than a commit dump.
   Link Unreleased to the GitLab comparison from 0.1.0a to main, and the initial
   release to its tag/release page. Post-alpha work belongs under Unreleased;
   local glibc verification must not be described as an already-published download.
3. Rewrite README.md toward roughly 100–150 lines, using the inventory above.
   Show a small TOML task example, how to invoke it, current download choices and
   accurate platform caveats; link full syntax, build/install/file workflows,
   development checks, packaging and CHANGELOG.md. Keep detailed material available
   rather than deleting useful guidance. The line range is a review target, not a gate.
4. Add a candid why/when section: TATR suits teams without dedicated CI/CD
   infrastructure that want a small, minimal local task orchestrator instead of
   maintaining orchestration in a large shell script. Explain that small shell
   scripts can be simpler for straightforward workflows, and Ansible targeting
   localhost is a reasonable alternative when it already fits the team's needs.
   Preserve the distinction between TOML orchestration and procedural work in
   programs/scripts; make no universal-replacement, benchmark or tiny-device claims.
5. Check the wording against actual capabilities and release assets. Keep the
   changelog as the ongoing change record and release notes as the announcement
   for a particular version. Add a brief documented maintenance convention for
   future notable changes, without introducing hooks, generators or CI machinery.
6. Validate local/remote documentation links and the selected copyable examples.
   Review that moved sections remain reachable, dates/versions/platform claims are
   correct, and the README stays readable. Report the changes and limitations for
   review; use checks appropriate to documentation changes. No task-runner code,
   tag/release mutation or publication is included; commit/push needs authorization.

## Scope boundary

Approved documentation changes only. No runtime changes,
new production dependencies, automation framework, retroactive release changes,
version-policy migration or feature expansion.
