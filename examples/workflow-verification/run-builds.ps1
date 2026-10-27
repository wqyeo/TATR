param(
    [Parameter(Mandatory)][string]$LinuxTatr,
    [Parameter(Mandatory)][string]$WindowsTatr,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [string]$Image = 'localhost/tatr-workflows:debian12'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$linuxBinary = (Resolve-Path -LiteralPath $LinuxTatr).Path
$windowsBinary = (Resolve-Path -LiteralPath $WindowsTatr).Path
$destination = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $destination) { throw 'Use a new output directory' }
New-Item -ItemType Directory -Path $destination | Out-Null
foreach ($directory in @('builds','linux-files','windows-files')) {
    New-Item -ItemType Directory -Path "$destination/$directory" | Out-Null
}
Copy-Item -LiteralPath "$PSScriptRoot/node","$PSScriptRoot/php" -Destination "$destination/builds" -Recurse
Copy-Item -LiteralPath "$PSScriptRoot/.env.example","$PSScriptRoot/builds.toml","$PSScriptRoot/check-builds.py","$PSScriptRoot/check-build-archives.py" -Destination "$destination/builds"
foreach ($directory in @('linux-files','windows-files')) {
    Copy-Item -LiteralPath "$PSScriptRoot/files.toml","$PSScriptRoot/files.cmake" -Destination "$destination/$directory"
}
Copy-Item -LiteralPath "$PSScriptRoot/linux-check.sh" -Destination $destination
Get-FileHash -LiteralPath $linuxBinary,$windowsBinary | Select-Object Path,Hash |
    ConvertTo-Json | Set-Content -LiteralPath "$destination/binaries.json"
& podman image inspect $Image --format '{{.Id}} {{.Digest}}' > "$destination/image.txt"
if ($LASTEXITCODE -ne 0) { throw 'Tools image not available' }
$binaryDirectory = Split-Path -Parent $linuxBinary
& podman run --rm --network none -v "${binaryDirectory}:/runner:ro" -v "${destination}:/work:rw" $Image sh /work/linux-check.sh > "$destination/linux-results.log" 2>&1
if ($LASTEXITCODE -ne 0) { throw "Linux checks failed; inspect $destination" }
Push-Location "$destination/windows-files"
try {
    & $windowsBinary --file files.toml run files > files-stdout.log 2> files-console.log
    if ($LASTEXITCODE -ne 0) { throw 'Windows file workflow failed' }
    $console = (Get-Content -LiteralPath files-console.log) -join "`n"
    $log = (Get-Content -LiteralPath logs/file-actions.log) -join "`n"
    if ($console -cne $log -or $log -notmatch 'event=run_finished.*outcome=success') {
        throw 'Windows logging mismatch'
    }
    if ((Get-Content files-stdout.log -Raw) -notmatch 'content comparisons passed') {
        throw 'Windows comparison did not complete'
    }
} finally { Pop-Location }
Write-Output "Builds, archives and logging passed; evidence: $destination"
