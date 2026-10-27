# Practical workflow verification

These opt-in fixtures exercise the existing runner with real external tools.
They add no runtime dependencies to TATR. See [recorded results](verification.md)
for the tested versions and outcomes; the examples are not a production backup
service or certification for every application framework/database version.

## Build, stage and archive applications

[builds.toml](../examples/workflow-verification/builds.toml) expresses this graph:

```text
npm ci -> Node default build -> Node custom output ----+
                                                      +-> stage -> archive-builds
Composer install -> validate -> lint -> PHP packaging -+
```

Both projects have a committed lockfile and a small local dependency. Installation
runs offline, so this checks npm/Composer invocation and dependency use without
depending on a public package registry. Node invokes an application build script;
PHP invokes a Composer script that produces an application artifact. These are
minimal fixtures, not webpack/Vite/Laravel/Symfony compatibility claims.

The task's `cwd` chooses where the program runs. The build script's argument
chooses its output directory; TATR does not redirect build products itself.
Default and custom outputs are checked against known content, copied/moved into
staging, and archived as ZIP and tar.gz. Archives are moved to another directory,
extracted and compared by relative filename and SHA-256. Paths include spaces.

[files.toml](../examples/workflow-verification/files.toml) separately exercises
portable CMake copy/rename/archive/extraction on Linux and Windows, including an
empty file. All fixtures require a fresh output directory and retain their evidence.

## Database backup and restore

[databases.toml](../examples/workflow-verification/databases.toml) uses this graph
for each engine:

```text
seed -> dump -> archive -> restore -> verify
```

TATR launches explicit Windows PowerShell scripts that invoke the vendor clients
through `podman exec`. No database ports are published; the disposable servers
use `--network none` and their clients connect inside the same container.

| Engine | Backup/export and restore |
| --- | --- |
| SQL Server | `sqlcmd` with `BACKUP DATABASE ... WITH CHECKSUM`, `RESTORE VERIFYONLY`, then restore into a new database with distinct data/log files. |
| PostgreSQL | `pg_dump -Fc`, then `pg_restore --exit-on-error` into a new database. |
| MySQL | `mysqldump --single-transaction`, then `mysql` importing into a new database. |
| MariaDB | `mariadb-dump --single-transaction`, then the matching `mariadb` client importing into a new database. |
| SQLite | `sqlite3 .backup`/`.restore` plus an independent `.dump`/SQL restore; check both restored databases. |

SQL Server writes its backup at the server; the script copies it out before
archiving, then copies the extracted backup back for restoration. Every engine's
backup is compressed, moved, extracted and hash-checked before restore. The small
fixture contains a primary-key table, three known rows (including spaces and an
apostrophe), and a view. Verification compares column/type/nullability metadata,
constraints, ordered rows and view results with the source and expected values.
SQLite additionally runs `PRAGMA integrity_check` on both restores.

The scripts set database-client error options explicitly: `sqlcmd -b -V 11`,
`psql -v ON_ERROR_STOP=1`, `pg_restore --exit-on-error`, and `sqlite3 -bail`.
MySQL/MariaDB imports do not use `--force`. A deliberately invalid query must
produce a failing client exit, TATR exit 1 and no dependent marker task.

These choices follow the vendor documentation for
[SQL Server backups](https://learn.microsoft.com/en-us/sql/linux/business-continuity/backup-restore/database-backup-restore?view=sql-server-ver17),
[pg_dump](https://www.postgresql.org/docs/15/app-pgdump.html),
[pg_restore](https://www.postgresql.org/docs/15/app-pgrestore.html),
[mysqldump](https://dev.mysql.com/doc/refman/8.4/en/mysqldump.html),
[mariadb-dump](https://mariadb.com/docs/server/clients-and-utilities/backup-restore-and-import-clients/mariadb-dump)
and the [SQLite CLI](https://sqlite.org/cli.html).
`--single-transaction` has storage-engine and concurrent-DDL limitations; this
fixture does not prove hot-backup consistency for a production workload.
PostgreSQL's single-database dump does not include cluster-wide roles/tablespaces.

## Reproduce on this Windows/Podman setup

Requirements: PowerShell 7 for the harness, native Windows PowerShell 5.1 for the
database tasks, Podman with an x86-64 Linux guest, native CMake 3.25+ on PATH, and
Windows/Linux TATR binaries. The Linux runner must work with Debian 12/glibc;
see the [local glibc recipe](packaging.md). The published Alpine binary is for
musl and is not the binary used in this Debian tools container.

From the checkout root, build the tools image and pull the database images:

```powershell
podman build -t localhost/tatr-workflows:debian12 -f examples/workflow-verification/Containerfile examples/workflow-verification
podman pull docker.io/library/postgres:15-bookworm
podman pull docker.io/library/mysql:8.4
podman pull docker.io/library/mariadb:11.4
podman pull mcr.microsoft.com/mssql/server:2022-latest
```

Supply your installed binaries; output directories must not already exist:

```powershell
mise exec -- pwsh -NoProfile -File examples/workflow-verification/run-builds.ps1 `
  -LinuxTatr 'path/to/linux/tatr' -WindowsTatr 'path/to/windows/tatr.exe' `
  -OutputDirectory build/workflow-verification/my-build-check

mise exec -- pwsh -NoProfile -File examples/workflow-verification/run-databases.ps1 `
  -Tatr 'path/to/windows/tatr.exe' `
  -OutputDirectory build/workflow-verification/my-database-check
```

`mise exec` is optional if native CMake is already on PATH. Database tests run
sequentially and use only synthetic data, including a SQL Server Developer
container with its EULA acceptance set in the test harness. Check available memory
before running: the harness caps that container at 4 GiB and does not resize the
VM. The inspected WSL guest had about 15 GiB available despite the machine-list
metadata reporting 2 GiB.

Random fixture credentials are written only to transient files in the output
directory and removed in `finally`; TATR tasks load their connection variables
from the generated `.env`. The harness removes only its uniquely named containers
and anonymous volumes. If interrupted forcibly, inspect `container-id.txt` and
remove the specific remaining fixture resources and transient credentials.
Images and nonsecret logs/artifacts remain for inspection. Do not use real
credentials or production databases with these fixtures.

TATR's native-executable check rejected this PC's Store-packaged `pwsh` alias
and packaged executable. The database TOML therefore uses the native
`powershell.exe` interpreter. Its script explicitly loads the matching inbox
utility module when launched from a PowerShell 7 environment. No runner change
was made to work around this platform limitation.

## Evidence and limits

Each run records binary hashes and image IDs/digests. Database results include
server/client versions, backup hashes, schema/data comparisons and a JSON result
summary. Tool images pin a Debian base, but APT packages and database tags can
change; the recorded versions/digests describe the actual run.

TATR action logs contain starts, finishes, outcomes and durations. Child output
is inherited, so the harness captures stdout and console stderr separately.
Console stderr can contain both action records and child progress/errors; only
action records are compared with the configured logfile. The build failure and
each database's SQL failure must prevent its dependent marker from running.

Container checks cover Linux tools orchestrated by Linux/Windows TATR; they do
not certify native Windows database servers, macOS, older server versions,
concurrent writes, whole-cluster recovery, encryption, permissions/ownership for
production accounts, retention, scheduling or point-in-time recovery. Use tested
restore procedures and vendor guidance when adapting these examples.
