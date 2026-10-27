param(
    [Parameter(Mandatory)][string]$Tatr,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('postgres','mysql','mariadb','sqlite','mssql')]
    [string[]]$Engines = @('postgres','mysql','mariadb','sqlite','mssql'),
    [string]$ToolsImage = 'localhost/tatr-workflows:debian12'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$binary = (Resolve-Path -LiteralPath $Tatr).Path
$destination = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $destination) { throw 'Use a new output directory' }
New-Item -ItemType Directory -Path $destination | Out-Null
Get-FileHash -LiteralPath $binary | Select-Object Path,Hash | ConvertTo-Json |
    Set-Content -LiteralPath "$destination/binary.json"
$images = @{
    postgres = 'docker.io/library/postgres:15-bookworm'
    mysql = 'docker.io/library/mysql:8.4'
    mariadb = 'docker.io/library/mariadb:11.4'
    sqlite = $ToolsImage
    mssql = 'mcr.microsoft.com/mssql/server:2022-latest'
}
$results = @()
$savedEngine = $env:TATR_TEST_ENGINE
$savedContainer = $env:TATR_TEST_CONTAINER
$savedPg = $env:PGPASSWORD
$savedMy = $env:MYSQL_PWD
$savedSql = $env:SQLCMDPASSWORD
try {
    foreach ($engine in $Engines) {
        $directory = Join-Path $destination $engine
        New-Item -ItemType Directory -Path $directory | Out-Null
        Copy-Item -LiteralPath "$PSScriptRoot/databases.toml","$PSScriptRoot/database-step.ps1" -Destination $directory
        $container = "tatr-workflow-$engine-$([guid]::NewGuid().ToString('N').Substring(0,8))"
        $password = "Fixture!$([guid]::NewGuid().ToString('N'))a9"
        $env:TATR_TEST_ENGINE = $engine
        $env:TATR_TEST_CONTAINER = $container
        $env:PGPASSWORD = $password
        $env:MYSQL_PWD = $password
        $env:SQLCMDPASSWORD = $password
        $dotenv = @("TATR_TEST_ENGINE=$engine", "TATR_TEST_CONTAINER=$container",
            "PGPASSWORD=$password", "MYSQL_PWD=$password", "SQLCMDPASSWORD=$password")
        # Transient, ignored file. Values are not printed and file is removed in finally.
        $dotenv | Set-Content -LiteralPath "$directory/.env" -Encoding utf8
        $containerEnv = @("POSTGRES_PASSWORD=$password", "MYSQL_ROOT_PASSWORD=$password",
            "MARIADB_ROOT_PASSWORD=$password", "MSSQL_SA_PASSWORD=$password", 'ACCEPT_EULA=Y', 'MSSQL_PID=Developer')
        $containerEnv | Set-Content -LiteralPath "$directory/container.env" -Encoding utf8
        $created = $false
        Push-Location $directory
        try {
            & podman image inspect $images[$engine] --format '{{.Id}} {{.Digest}}' > image.txt
            if ($LASTEXITCODE -ne 0) { throw 'Pull/build the required image first' }
            # No ports or host volumes: all database state is disposable container storage.
            $options = @('run','-d','--name',$container,'--network','none','--env-file','container.env')
            if ($engine -eq 'mssql') { $options += @('--memory','4g') }
            $options += $images[$engine]
            if ($engine -eq 'sqlite') { $options += @('sleep','infinity') }
            & podman @options > container-id.txt
            if ($LASTEXITCODE -ne 0) { throw 'Container creation failed' }
            $created = $true
            $ready = $false
            for ($attempt = 0; $attempt -lt 90; $attempt++) {
                & pwsh -NoProfile -File ./database-step.ps1 ready > readiness.log 2>&1
                if ($LASTEXITCODE -eq 0) { $ready = $true; break }
                Start-Sleep -Seconds 2
            }
            if (-not $ready) { throw 'Database did not become ready within the bounded wait' }
            & pwsh -NoProfile -File ./database-step.ps1 versions > client-versions.txt 2>&1
            if ($LASTEXITCODE -ne 0) { throw 'Version check failed' }
            # Clear connection variables so the task must load them from dotenv.
            $env:TATR_TEST_ENGINE = $null
            $env:TATR_TEST_CONTAINER = $null
            $env:PGPASSWORD = $null
            $env:MYSQL_PWD = $null
            $env:SQLCMDPASSWORD = $null
            & $binary --file databases.toml list > list.txt
            if ($LASTEXITCODE -ne 0) { throw 'List failed' }
            & $binary --file databases.toml plan verify > plan.txt
            if ($LASTEXITCODE -ne 0 -or (Test-Path logs/database-actions.log)) { throw 'Read-only planning failed' }
            & $binary --file databases.toml run verify > stdout.log 2> console.log
            if ($LASTEXITCODE -ne 0) { throw 'Backup/restore workflow failed; see console.log' }
            $console = (Get-Content console.log | Where-Object { $_ -match '^time=' }) -join "`n"
            if ($console -cne ((Get-Content logs/database-actions.log) -join "`n") -or
                $console -notmatch 'event=run_finished.*outcome=success') { throw 'Action log mismatch' }
            if ($console -match 'schema, constraints, rows and view results match') { throw 'Child output leaked into action log' }
            & $binary --file databases.toml run must-not-run > failure-stdout.log 2> failure-console.log
            if ($LASTEXITCODE -ne 1 -or (Test-Path database-failure-marker)) { throw 'SQL failure did not stop dependents' }
            $failure = Get-Content failure-console.log -Raw
            if ($failure -notmatch 'task="sql-error".*outcome=child_failure' -or
                $failure -match 'event=task_started task="must-not-run"') { throw 'Missing SQL failure action record' }
            $results += [pscustomobject]@{ engine=$engine; result='PASS'; detail='restore/schema/data/archive/logging/failure' }
            Write-Output "$engine PASS"
        } catch {
            $results += [pscustomobject]@{ engine=$engine; result='FAIL'; detail="$($_.Exception.Message)" }
            Write-Output "$engine FAIL: $($_.Exception.Message)"
        } finally {
            if ($created) {
                & podman logs $container > server.log 2>&1
                & podman rm -f -v $container > cleanup.log 2>&1
                if ($LASTEXITCODE -ne 0) { throw "Could not remove fixture container $container" }
            }
            Remove-Item -LiteralPath .env,container.env -ErrorAction SilentlyContinue
            Pop-Location
        }
        $results | ConvertTo-Json -AsArray | Set-Content -LiteralPath "$destination/results.json"
    }
} finally {
    $env:TATR_TEST_ENGINE = $savedEngine
    $env:TATR_TEST_CONTAINER = $savedContainer
    $env:PGPASSWORD = $savedPg
    $env:MYSQL_PWD = $savedMy
    $env:SQLCMDPASSWORD = $savedSql
}
if (@($results | Where-Object result -ne 'PASS').Count) { throw "Some scenarios failed; inspect $destination/results.json" }
