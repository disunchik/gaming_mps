# Activate the installed ESP-IDF v5.5.1 without embedding a user's home directory.
$ErrorActionPreference = 'Stop'
$env:PATHEXT = [Environment]::GetEnvironmentVariable('PATHEXT', 'Machine') + ';.CPL'
if ($env:PATH -notlike '*;*') {
    $env:PATH = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}
if ($env:GAMING_IDF_ACTIVATE) {
    . $env:GAMING_IDF_ACTIVATE
} elseif ($env:IDF_PATH -and $env:IDF_PYTHON_ENV_PATH -and (Test-Path "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe")) {
    $env:PATH = "$env:IDF_PYTHON_ENV_PATH\Scripts;$env:PATH"
    # EIM may use a flat tool directory; an already active EIM terminal is valid
    # without re-exporting it with the standard installer's different layout.
    if (!(Get-Command xtensa-esp32-elf-gcc.exe -ErrorAction SilentlyContinue)) {
        . "$env:IDF_PATH\export.ps1"
    }
} else {
    $candidates = @('D:\Espressif\eim_idf.json', 'C:\Espressif\eim_idf.json', 'C:\Espressif\tools\eim_idf.json',
        (Join-Path $env:USERPROFILE '.espressif\tools\eim_idf.json'))
    if ($env:IDF_TOOLS_PATH) { $candidates = @((Join-Path $env:IDF_TOOLS_PATH 'eim_idf.json')) + $candidates }
    $profile = $null
    foreach ($candidate in $candidates) {
        if (!(Test-Path $candidate)) { continue }
        $registry = Get-Content $candidate -Raw | ConvertFrom-Json
        $installation = $registry.idfInstalled | Where-Object { $_.name -eq 'v5.5.1' } | Select-Object -First 1
        if ($installation -and (Test-Path $installation.activationScript)) {
            $profile = $installation.activationScript
            break
        }
    }
    if (!$profile) { throw 'Open an ESP-IDF v5.5.1 terminal, or set GAMING_IDF_ACTIVATE to its activation script.' }
    . $profile
}
if (!(Test-Path "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe")) { throw 'ESP-IDF Python environment is unavailable.' }
function global:Invoke-GamingIdf {
    & "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$env:IDF_PATH\tools\idf.py" @args
}
Set-Alias -Name idf.py -Value Invoke-GamingIdf -Scope Global -Force
$version = idf.py --version
if ($LASTEXITCODE -ne 0 -or $version -notmatch 'v5\.5\.1(?:\s|$)') { throw "Expected ESP-IDF v5.5.1, found: $version" }
Write-Host $version
