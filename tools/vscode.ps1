param([ValidateSet('Setup', 'Start', 'Stop')][string]$Action = 'Setup')
$ErrorActionPreference = 'Stop'
$env:PATHEXT = [Environment]::GetEnvironmentVariable('PATHEXT', 'Machine')
$root = Split-Path $PSScriptRoot -Parent
$stateFile = Join-Path $root 'artifacts\vscode-qemu.json'

function Stop-OwnedQemu {
    if (!(Test-Path $stateFile)) { return }
    $record = Get-Content $stateFile -Raw | ConvertFrom-Json
    $owned = Get-Process -Id $record.pid -ErrorAction SilentlyContinue
    if ($owned -and $owned.StartTime.ToUniversalTime().Ticks.ToString() -eq $record.started) {
        & "$env:SystemRoot\System32\taskkill.exe" /PID $record.pid /T /F | Out-Null
        if ($LASTEXITCODE -ne 0) { throw 'Could not stop the owned QEMU launcher.' }
    }
    Remove-Item $stateFile
}

if ($Action -eq 'Stop') { Stop-OwnedQemu; exit 0 }
. "$PSScriptRoot\enter.ps1"
Push-Location $root
try {
    if ($Action -eq 'Setup') {
        $gdb = (Get-Command xtensa-esp32-elf-gdb.exe -ErrorAction Stop).Source
        $configuration = [ordered]@{
            name = 'ESP32 / QEMU'
            type = 'cppdbg'
            request = 'launch'
            program = '${workspaceFolder}/build/gaming_mps.elf'
            cwd = '${workspaceFolder}'
            MIMode = 'gdb'
            # MIDebugEngine has no Xtensa enum. This selects its 32-bit client
            # layout; the connected GDB target remains Xtensa (no set architecture).
            targetArchitecture = 'arm'
            miDebuggerPath = $gdb
            miDebuggerArgs = '--nx'
            externalConsole = $false
            setupCommands = @()
            customLaunchSetupCommands = @(
                @{ text = '-file-exec-and-symbols "${workspaceFolder}/build/gaming_mps.elf"'; ignoreFailures = $false },
                @{ text = '-target-select remote 127.0.0.1:3334'; ignoreFailures = $false },
                @{ text = '-interpreter-exec console "monitor system_reset"'; ignoreFailures = $false },
                @{ text = '-break-insert -t app_main'; ignoreFailures = $false }
            )
            launchCompleteCommand = 'exec-continue'
            preLaunchTask = 'QEMU: start debugger'
            postDebugTask = 'QEMU: stop debugger'
        }
        $path = Join-Path $root '.vscode\launch.json'
        $launch = if (Test-Path $path) { Get-Content $path -Raw | ConvertFrom-Json } else {
            [pscustomobject]@{ version = '0.2.0'; configurations = @() }
        }
        $launch.configurations = @($launch.configurations | Where-Object { $_.name -ne $configuration.name }) + @($configuration)
        $launch | ConvertTo-Json -Depth 12 | Set-Content $path -Encoding UTF8
        Write-Host 'Ready. Open this project folder in VS Code, select ESP32 / QEMU and press F5.'
        Write-Host 'The C/C++ extension (ms-vscode.cpptools) must be installed.'
        Write-Host 'F10 steps over a line. Shift+F5 stops this debug session.'
        exit 0
    }

    Stop-OwnedQemu
    foreach ($port in @(5558, 3334)) {
        $listener = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $port)
        try { $listener.Start() } finally { $listener.Stop() }
    }
    idf.py -DIDF_TARGET=esp32 build
    if ($LASTEXITCODE -ne 0) { throw 'ESP32 build failed.' }
    New-Item -ItemType Directory -Path (Join-Path $root 'artifacts') -Force | Out-Null
    $python = "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe"
    $launcher = Start-Process -FilePath $python -ArgumentList @(
        ('"' + (Join-Path $PSScriptRoot 'run.py') + '"'),
        '--no-build', '--debug', '--uart-port', '5558', '--gdb-port', '3334'
    ) -WorkingDirectory $root -PassThru
    @{ pid = $launcher.Id; started = $launcher.StartTime.ToUniversalTime().Ticks.ToString() } |
        ConvertTo-Json | Set-Content $stateFile -Encoding UTF8
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    do {
        Start-Sleep -Milliseconds 250
        $launcher.Refresh()
        if ($launcher.HasExited) { throw 'QEMU launcher exited. Inspect artifacts/qemu-5558/process.log.' }
        $ready = Get-NetTCPConnection -LocalPort 3334 -State Listen -ErrorAction SilentlyContinue |
            Where-Object { $_.OwningProcess -ne $PID }
    } until ($ready -or [DateTime]::UtcNow -gt $deadline)
    if (!$ready) { Stop-OwnedQemu; throw 'QEMU did not open the GDB server within 30 seconds.' }
    Write-Host 'QEMU ready for VS Code at 127.0.0.1:3334.'
} catch {
    if ($Action -eq 'Start') { Stop-OwnedQemu }
    throw
} finally { Pop-Location }
