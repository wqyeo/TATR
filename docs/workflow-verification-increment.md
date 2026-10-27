# Increment status: practical workflow verification

Status: approved and completed locally. All planned scenarios passed; see the
[workflow guide](workflow-verification.md) and [results](verification.md).
Existing local changes are preserved. Changes remain uncommitted; commit/push
requires authorization. No further implementation increment has been selected.

## Approved plan

1. Inventory Podman resources and installed TATR artifacts. Use disposable test
   containers, synthetic data, private networking and credentials, and a dedicated
   build/workflow-verification/ evidence directory. Record binary hashes, tool/server
   versions and image digests. Run database servers one at a time. The current VM
   reports 2 GiB RAM: check SQL Server requirements before starting it. Do not resize
   or restart the shared VM without approval; report resource blockers explicitly.
2. Add small opt-in TOML examples and fixture/scripts under a dedicated examples/
   directory. Keep orchestration in TOML and procedural checks in scripts. Execute
   substantive commands through TATR, exercising cwd and dotenv. Use the existing
   installed/released binaries; add no production dependencies or testing framework.
3. Test a minimal Node.js project with npm dependency installation and a build,
   and a minimal PHP project with Composer installation, validation and an explicit
   application packaging/build step. Verify default and configured output paths,
   then move/copy artifacts to a separate staging directory. State exactly which
   projects/tools were tested: PHP has no universal build operation, and these
   fixtures do not certify every framework.
4. Create ZIP and tar.gz archives, move them to another directory, extract them
   and compare paths/content with the originals. Include paths with spaces. Check
   console and configured file action logs. Capture child output explicitly through
   tool options or scripts and distinguish it from TATR's action logfile.
5. Seed each database with deterministic data, back up/export through TATR, restore
   into a separate database and compare schema and representative data. Use:
   sqlcmd with BACKUP/RESTORE for SQL Server; pg_dump/pg_restore for PostgreSQL;
   mysqldump/mysql for MySQL; mariadb-dump/mariadb for MariaDB; and sqlite3 .backup
   plus .dump/SQL restore for SQLite. Ensure SQL errors cause failing process exits.
   Account for SQL Server backup paths being server-side. Exercise archiving,
   moving and extracting generated backups before restore where applicable. Use
   vendor-documented options and disposable databases, never copy live data files.
6. Test representative build and database command failures. Verify TATR returns
   failure, dependent archive/move tasks do not run, and action logs identify the
   failure. Keep credentials out of checked-in fixtures and collected logs. Remove
   only resources created for these tests; retain useful evidence under build/.
7. Add a workflow guide, docs-index link, verification results and an appropriate
   Unreleased changelog entry. Report pass/fail/blocked for every scenario, exact
   tested environments and limitations. Use scoped checks, not performance tests
   or an unrelated full compiler matrix. If a runner defect needs C++ changes,
   present a fix plan before implementing it. Commit/push and publication require
   separate authorization.

## Scope boundary

Functional integration checks using synthetic local data. No production databases,
backup scheduling, retention service, cloud uploads, database adapters, runtime
feature work or release mutation. Container checks primarily verify Linux; repeat
portable file/logging checks with the Windows binary where practical. Do not imply
macOS or untested database-version coverage. Restore checks establish more than
successful command exits, but do not certify a production disaster-recovery plan.
