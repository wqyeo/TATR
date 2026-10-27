param(
    [Parameter(Mandatory)][ValidateSet('ready','versions','seed','dump','archive','restore','verify','sql-error','marker')]
    [string]$Step
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# PS7 can pass its module path to a native-launched Windows PowerShell child.
# Load the matching inbox utility module explicitly (Get-FileHash is a function).
if ($PSVersionTable.PSEdition -eq 'Desktop') {
    Import-Module "$PSHOME/Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1" -Force
}
$engine = $env:TATR_TEST_ENGINE
$container = $env:TATR_TEST_CONTAINER
if ($engine -notin @('postgres','mysql','mariadb','sqlite','mssql') -or
    $container -notmatch '^tatr-workflow-[a-z0-9-]+$') {
    throw 'Only the disposable workflow fixture containers are accepted'
}

function Invoke-Podman([string[]]$Arguments) {
    $previousPreference = $ErrorActionPreference
    try {
        # Windows PowerShell otherwise turns native stderr into a terminating error
        # before we can inspect the actual client exit status.
        $ErrorActionPreference = 'Continue'
        $output = & podman @Arguments 2>&1
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $previousPreference }
    $result = ($output | ForEach-Object { "$_" }) -join "`n"
    # Never record argument/environment lists; output contains only fixture data.
    Add-Content -LiteralPath tool-output.log -Value $result -Encoding utf8
    if ($code -ne 0) { throw "Database tool exited ${code}: $result" }
    return $result.Trim()
}

function Query([string]$Database, [string]$Sql) {
    switch ($engine) {
        postgres {
            return Invoke-Podman @('exec','--env','PGPASSWORD',$container,'psql','-h','127.0.0.1','-U','postgres',
                '-X','-A','-t','-v','ON_ERROR_STOP=1','-d',$Database,'-c',$Sql)
        }
        { $_ -in 'mysql','mariadb' } {
            $client = if ($engine -eq 'mysql') { 'mysql' } else { 'mariadb' }
            return Invoke-Podman @('exec','--env','MYSQL_PWD',$container,$client,'-h','127.0.0.1','-u','root',
                '--batch','--skip-column-names',"--database=$Database","--execute=$Sql")
        }
        mssql {
            # CREATE VIEW must start its own batch; other queries suppress counts
            # so ordered result comparisons contain data only.
            $statement = if ($Sql -match '^\s*CREATE VIEW') { $Sql } else { "SET NOCOUNT ON; $Sql" }
            return Invoke-Podman @('exec','--env','SQLCMDPASSWORD',$container,'/opt/mssql-tools18/bin/sqlcmd',
                '-S','localhost','-U','sa','-C','-b','-V','11','-l','3','-t','30','-h','-1','-W','-s','|','-d',$Database,
                '-Q',$statement)
        }
        sqlite {
            return Invoke-Podman @('exec',$container,'sqlite3','-batch','-bail',"/tmp/$Database.db",$Sql)
        }
    }
}

$admin = switch ($engine) { postgres { 'postgres' } mssql { 'master' } sqlite { 'tatr_source' } default { 'mysql' } }
$extension = switch ($engine) { postgres { 'dump' } mssql { 'bak' } sqlite { 'db' } default { 'sql' } }
$backup = "backup.$extension"
$remote = if ($engine -eq 'mssql') { "/var/opt/mssql/data/tatr.$extension" } else { "/tmp/tatr.$extension" }

switch ($Step) {
    ready { [void](Query $admin 'SELECT 1;') }
    versions {
        $sql = if ($engine -eq 'sqlite') { 'SELECT sqlite_version();' } elseif ($engine -eq 'mssql') {
            'SELECT @@VERSION;'
        } else { 'SELECT version();' }
        Query $admin $sql | Set-Content -LiteralPath versions.txt -Encoding utf8
        switch ($engine) {
            postgres { Invoke-Podman @('exec',$container,'pg_dump','--version') }
            mysql { Invoke-Podman @('exec',$container,'mysqldump','--version') }
            mariadb { Invoke-Podman @('exec',$container,'mariadb-dump','--version') }
            sqlite { Invoke-Podman @('exec',$container,'sqlite3','--version') }
            mssql { Invoke-Podman @('exec',$container,'/opt/mssql-tools18/bin/sqlcmd','-?') }
        }
    }
    seed {
        if ($engine -ne 'sqlite') {
            [void](Query $admin 'CREATE DATABASE tatr_source;')
            if ($engine -ne 'mssql') { [void](Query $admin 'CREATE DATABASE tatr_restored;') }
        }
        [void](Query 'tatr_source' "CREATE TABLE items (id INTEGER PRIMARY KEY, label VARCHAR(80) NOT NULL, amount INTEGER NOT NULL);
INSERT INTO items VALUES (1,'alpha',42),(2,'two words',-7),(3,'quote''s',0);")
        [void](Query 'tatr_source' 'CREATE VIEW positive_items AS SELECT id,label,amount FROM items WHERE amount > 0;')
        Write-Output "$engine fixture seeded"
    }
    dump {
        switch ($engine) {
            postgres {
                [void](Invoke-Podman @('exec','--env','PGPASSWORD',$container,'pg_dump','-h','127.0.0.1',
                    '-U','postgres','-Fc','--file',$remote,'tatr_source'))
            }
            { $_ -in 'mysql','mariadb' } {
                $tool = if ($engine -eq 'mysql') { 'mysqldump' } else { 'mariadb-dump' }
                $options = @('exec','--env','MYSQL_PWD',$container,$tool,'-h','127.0.0.1','-u','root',
                    '--single-transaction','--routines','--events','--triggers',"--result-file=$remote")
                if ($engine -eq 'mysql') { $options += '--set-gtid-purged=OFF' }
                [void](Invoke-Podman ($options + 'tatr_source'))
            }
            mssql {
                [void](Query $admin "BACKUP DATABASE tatr_source TO DISK=N'$remote' WITH INIT, CHECKSUM;")
            }
            sqlite {
                [void](Query 'tatr_source' ".backup '$remote'")
                [void](Invoke-Podman @('exec',$container,'sh','-ec',
                    'sqlite3 -bail /tmp/tatr_source.db .dump > /tmp/export.sql'))
                [void](Invoke-Podman @('cp',"${container}:/tmp/export.sql",'export.sql'))
            }
        }
        [void](Invoke-Podman @('cp',"${container}:$remote",$backup))
        if ((Get-Item -LiteralPath $backup).Length -eq 0) { throw 'Empty backup' }
        Write-Output "$engine backup exported"
    }
    archive {
        New-Item -ItemType Directory -Path 'archive destination','restored files' | Out-Null
        $files = @($backup)
        if ($engine -eq 'sqlite') { $files += 'export.sql' }
        $hashes = @{}
        foreach ($file in $files) { $hashes[$file] = (Get-FileHash -LiteralPath $file).Hash }
        & cmake -E tar czf 'backup bundle.tar.gz' @files
        if ($LASTEXITCODE -ne 0) { throw 'Backup archive failed' }
        # Explicit workspace-contained destination; no recursive moves/deletes.
        Move-Item -LiteralPath 'backup bundle.tar.gz' -Destination 'archive destination/backup bundle.tar.gz'
        Push-Location 'restored files'
        try {
            & cmake -E tar xzf '../archive destination/backup bundle.tar.gz'
            if ($LASTEXITCODE -ne 0) { throw 'Backup extraction failed' }
        } finally { Pop-Location }
        foreach ($file in $files) {
            if ((Get-FileHash -LiteralPath "restored files/$file").Hash -ne $hashes[$file]) {
                throw "Backup archive content mismatch: $file"
            }
        }
        $hashes | ConvertTo-Json | Set-Content -LiteralPath backup-hashes.json -Encoding utf8
        Write-Output 'Backup compressed, moved and extracted; hashes match'
    }
    restore {
        [void](Invoke-Podman @('cp',"restored files/$backup","${container}:$remote.restored"))
        switch ($engine) {
            postgres {
                [void](Invoke-Podman @('exec','--env','PGPASSWORD',$container,'pg_restore','-h','127.0.0.1',
                    '-U','postgres','--exit-on-error','--dbname','tatr_restored',"$remote.restored"))
            }
            { $_ -in 'mysql','mariadb' } {
                $client = if ($engine -eq 'mysql') { 'mysql' } else { 'mariadb' }
                # Explicit shell owns input redirection. No --force: SQL errors must fail.
                [void](Invoke-Podman @('exec','--env','MYSQL_PWD',$container,'sh','-ec',
                    "$client -h 127.0.0.1 -u root tatr_restored < $remote.restored"))
            }
            mssql {
                [void](Query $admin "RESTORE VERIFYONLY FROM DISK=N'$remote.restored' WITH CHECKSUM;")
                [void](Query $admin "RESTORE DATABASE tatr_restored FROM DISK=N'$remote.restored'
WITH MOVE N'tatr_source' TO N'/var/opt/mssql/data/tatr_restored.mdf',
MOVE N'tatr_source_log' TO N'/var/opt/mssql/data/tatr_restored_log.ldf', CHECKSUM;")
            }
            sqlite {
                [void](Query 'tatr_restored' ".restore '$remote.restored'")
                [void](Invoke-Podman @('cp','restored files/export.sql',"${container}:/tmp/export.restored.sql"))
                [void](Invoke-Podman @('exec',$container,'sh','-ec',
                    'sqlite3 -bail /tmp/tatr_sql_restored.db < /tmp/export.restored.sql'))
            }
        }
        Write-Output "$engine restored from extracted backup"
    }
    verify {
        $schema = if ($engine -eq 'sqlite') { '.schema' } else {
            "SELECT table_name,column_name,data_type,is_nullable,ordinal_position FROM information_schema.columns WHERE table_name IN ('items','positive_items')"
        }
        if ($engine -in 'mysql','mariadb') { $schema += ' AND table_schema=DATABASE()' }
        if ($engine -ne 'sqlite') { $schema += ' ORDER BY table_name,ordinal_position;' }
        $constraints = if ($engine -eq 'sqlite') { 'PRAGMA table_info(items);' } else {
            "SELECT table_name,constraint_type FROM information_schema.table_constraints WHERE table_name='items'"
        }
        if ($engine -in 'mysql','mariadb') { $constraints += ' AND table_schema=DATABASE()' }
        if ($engine -ne 'sqlite') { $constraints += ' ORDER BY constraint_type;' }
        $queries = @($schema,$constraints,'SELECT id,label,amount FROM items ORDER BY id;',
            'SELECT id,label,amount FROM positive_items ORDER BY id;')
        $expectedRows = @('1|alpha|42','2|two words|-7',"3|quote's|0") -join "`n"
        $sources = @('tatr_restored')
        if ($engine -eq 'sqlite') { $sources += 'tatr_sql_restored' }
        foreach ($database in $sources) {
            $report = @()
            foreach ($sql in $queries) {
                $before = Query 'tatr_source' $sql
                $after = Query $database $sql
                if (-not $before -or $before -cne $after) { throw "Schema/data mismatch: $sql" }
                $report += $after
            }
            $rows = ($report[2] -replace "`t",'|').Trim()
            if ($rows -cne $expectedRows) { throw "Unexpected fixture rows: $rows" }
            if (($report[3] -replace "`t",'|').Trim() -cne '1|alpha|42') { throw 'View mismatch' }
            if ($engine -eq 'sqlite' -and (Query $database 'PRAGMA integrity_check;') -ne 'ok') {
                throw 'SQLite integrity check failed'
            }
            $report | Set-Content -LiteralPath "$database-comparison.txt" -Encoding utf8
        }
        Write-Output "$engine schema, constraints, rows and view results match"
    }
    sql-error { [void](Query 'tatr_source' 'SELECT * FROM tatr_deliberately_missing_table;') }
    marker { Set-Content -LiteralPath database-failure-marker -Value 'must not run' }
}
