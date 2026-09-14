param([Parameter(Mandatory=$true)][string]$ZigPath)
$ErrorActionPreference = 'Stop'
$packageDir = Split-Path $PSScriptRoot -Parent
& $ZigPath c++ (Join-Path $PSScriptRoot 'recorder.cpp') -shared -o (Join-Path $packageDir 'IIDXRecorder.dll') -target x86_64-windows-gnu -O2 -static -std=c++17 -lmfplat -lmfuuid -lole32 -luuid
if ($LASTEXITCODE -ne 0) { throw 'DLL build failed' }
