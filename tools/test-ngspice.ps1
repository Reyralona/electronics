param([string]$Root = 'build/spice-local/ucrt64')
$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
$executable = Join-Path $resolvedRoot 'bin/ngspice_con.exe'
if (!(Test-Path -LiteralPath $executable)) { throw "ngspice não encontrado em $executable" }
$env:Path = "$(Join-Path $resolvedRoot 'bin');$env:Path"
$output = & $executable -b tests/spice/dc_reference.cir 2>&1
if ($LASTEXITCODE -ne 0) { throw "ngspice falhou: $output" }
$text = $output -join "`n"
if ($text -match '(?im)^\s*error\b' -or $text -match '(?i)couldn.t be loaded') { throw "Erro de configuração do ngspice: $text" }
if ($text -notmatch 'v\(1\)\s*=\s*5\.000000e\+00' -or $text -notmatch '@r1\[i\]\s*=\s*5\.000000e-03') { throw "Resultado inesperado: $text" }
Write-Output 'Referência DC: 5 V e 5 mA verificados.'
