# Next increment proposal: small-system resource baseline

Status: awaiting review. Do not implement until approved.

The basic runner now includes [runtime arguments](runtime-arguments.md), task
directories, and environment overrides. See [verification results](verification.md).
This measurement increment requires separate approval.

## Outcome

Establish repeatable executable-size, command-latency, and peak-memory measurements
to assess TATR's small-system goal and inform explicit budgets.

## Proposed steps

1. Define fixed generated workloads: --help, list/plan/group-only run with 1, 100,
   and 1,000 tasks, plus a separate no-op child-launch scenario. Keep all configs
   under the existing 1 MiB limit and use only fixtures beneath build/.
2. Add an opt-in developer measurement command for release binaries on native
   Windows and Linux. Keep measurement tools out of installation and production
   dependencies; do not change the runner's behavior or user configuration.
3. Run three untimed warmups and 30 fresh processes per scenario, redirecting
   output consistently. Record median and p95 end-to-end command latency. Time
   Linux commands inside the already-running container, excluding Podman startup
   and builds. Label results as warm-cache CLI latency, not cold-boot startup.
4. Measure native peak memory for scenarios that spawn no child, recording each
   platform's metric and units (Linux resident memory versus Windows working set).
   Keep child-launch timing separate. Record release bytes, runtime dependencies,
   and shared-library exclusions; do not equate different native memory metrics.
5. Emit machine-readable results with OS/architecture, toolchain/build settings,
   workload sizes, binary hash, and source revision/dirty state. Verify command
   failures cannot look like valid measurements and document reproducible commands.
6. Publish the Windows/Linux baseline in repository docs and recommend budgets
   based on observed results for review. Report macOS and tiny-device gaps;
   commit/push only when authorized. Budget enforcement needs separate approval.

## Scope boundary

No production behavior changes, optimizations, new execution features, production
dependencies, automatic performance gates, remote CI activation, or claims of
macOS/tiny-device validation without native evidence.
