$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\enter.ps1"
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$PSScriptRoot\..\tests\test_clipboard.py"
if ($LASTEXITCODE -ne 0) { throw 'Clipboard protocol verification failed.' }
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$PSScriptRoot\..\tests\native_output.py"
if ($LASTEXITCODE -ne 0) { throw 'Native Windows output verification failed.' }
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$PSScriptRoot\smoke.py"
if ($LASTEXITCODE -ne 0) { throw 'QEMU verification failed.' }
& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$PSScriptRoot\..\tests\native_console.py"
if ($LASTEXITCODE -ne 0) { throw 'Native Windows console verification failed.' }

& "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe" "$PSScriptRoot\..\games\tetris\tests\qemu_smoke.py" --expect-p2
if ($LASTEXITCODE -ne 0) { throw 'Tetris QEMU verification failed.' }
