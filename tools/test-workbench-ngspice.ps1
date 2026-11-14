param([string]$Root = 'build/spice-local/ucrt64')
$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
$env:Path = "$(Join-Path $resolvedRoot 'bin');C:\msys64\ucrt64\bin;$env:Path"
& gcc -std=c11 -Wall -Wextra -Werror -Isrc tests/test_netlist.c src/netlist.c src/workbench.c -o build/test_netlist.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& ./build/test_netlist.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$output = & (Join-Path $resolvedRoot 'bin/ngspice_con.exe') -b build/netlist-reference.cir 2>&1
if ($LASTEXITCODE -ne 0) { throw "ngspice falhou: $output" }
$text = $output -join "`n"
if ($text -match '(?im)^\s*error\b' -or $text -match '(?i)couldn.t be loaded') { throw "Erro do ngspice: $text" }
if ($text -notmatch 'v\(\d+\)\s*=\s*5\.000000e\+00' -or $text -notmatch '@vsource\[i\]\s*=\s*-5\.00000e-03') { throw "Resultado inesperado: $text" }
Write-Output 'Bancada -> netlist -> ngspice: 5 V e 5 mA verificados.'
