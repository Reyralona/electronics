param([string]$Root = 'build/spice-local/ucrt64')
$ErrorActionPreference = 'Stop'
foreach ($required in 'bin/ngspice_con.exe', 'bin/libngspice-0.dll', 'lib/ngspice/digital.cm') {
    if (!(Test-Path -LiteralPath (Join-Path $Root $required) -PathType Leaf)) {
        throw "ngspice incompleto: falta $required em $Root. Execute ./tools/setup-ngspice.ps1. Se o destino existir, mova-o para backup antes de preparar novamente."
    }
}
$resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
$executable = Join-Path $resolvedRoot 'bin/ngspice_con.exe'
$normalPath = $env:Path
try {
    $env:Path = "$env:SystemRoot\System32;$env:SystemRoot"
    $reference = Join-Path $PSScriptRoot '../tests/spice/dc_reference.cir'
    $output = & $executable -b $reference 2>&1
    if ($LASTEXITCODE -ne 0) { throw "ngspice falhou: $output" }
    $text = $output -join "`n"
    if ($text -match '(?im)^\s*error\b' -or $text -match '(?i)couldn.t be loaded') { throw "Erro de configuração do ngspice: $text" }
    if ($text -notmatch 'v\(1\)\s*=\s*5\.000000e\+00' -or $text -notmatch '@r1\[i\]\s*=\s*5\.000000e-03') { throw "Resultado inesperado: $text" }
} catch {
    throw "Runtime ngspice invalido: $($_.Exception.Message). Mova $Root para backup e execute ./tools/setup-ngspice.ps1."
} finally {
    $env:Path = $normalPath
}
Write-Output 'Referência DC: 5 V e 5 mA verificados.'
