param([string]$Root = 'build/spice-local/ucrt64')
$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
$dll = Join-Path $resolvedRoot 'bin/libngspice-0.dll'
if (!(Test-Path -LiteralPath $dll)) { throw "DLL ngspice não encontrada: $dll" }
$env:Path = "$(Join-Path $resolvedRoot 'bin');C:\msys64\ucrt64\bin;$env:Path"
$env:SPICE_LIB_DIR = (Join-Path $resolvedRoot 'share/ngspice').Replace('\','/')
& gcc -std=c11 -Wall -Wextra -Werror "-I$(Join-Path $resolvedRoot 'include')" experiments/ngspice_shared_probe.c -o build/spice-shared-probe.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/spice-shared-probe.exe $dll
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Output 'Interface compartilhada: referência DC validada.'
