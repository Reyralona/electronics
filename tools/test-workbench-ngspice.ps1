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
$snapped = & (Join-Path $resolvedRoot 'bin/ngspice_con.exe') -b 'build/netlist-snap.cir' 2>&1
if ($LASTEXITCODE -ne 0) { throw "Falha ngspice (encaixe): $snapped" }
$snappedText = $snapped -join "`n"
if ($snappedText -match '(?im)^\s*error\b' -or $snappedText -match '(?i)couldn.t be loaded') { throw "Erro do ngspice: $snappedText" }
if ($snappedText -notmatch '@vsource\[i\]\s*=\s*-5\.00000e-03') { throw "Resultado de encaixe inesperado: $snappedText" }
Write-Output 'Resistor encaixado na breadboard: 5 mA verificados.'
$adjustable = & (Join-Path $resolvedRoot 'bin/ngspice_con.exe') -b 'build/netlist-adjustable.cir' 2>&1
if ($LASTEXITCODE -ne 0) { throw "Falha ngspice (valores ajustados): $adjustable" }
$adjustableText = $adjustable -join "`n"
if ($adjustableText -match '(?im)^\s*error\b' -or $adjustableText -match '(?i)couldn.t be loaded') { throw "Erro do ngspice: $adjustableText" }
if ($adjustableText -notmatch 'v\(\d+\)\s*=\s*1\.000000e\+01' -or $adjustableText -notmatch '@vsource\[i\]\s*=\s*-5\.00000e-03') { throw "Valores ajustados inesperados: $adjustableText" }
Write-Output 'Valores ajustados: 10 V e 2 kOhm resultam em 5 mA.'
foreach ($case in @(@{ Name = 'forward'; Min = 0.002; Max = 0.004 }, @{ Name = 'reverse'; Min = 0; Max = 1e-8 })) {
    $result = & (Join-Path $resolvedRoot 'bin/ngspice_con.exe') -b "build/netlist-led-$($case.Name).cir" 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Falha ngspice ($($case.Name)): $result" }
    $value = ($result -join "`n")
    if ($value -match '(?im)^\s*error\b' -or $value -match '(?i)couldn.t be loaded') { throw "Erro do ngspice: $value" }
    if ($value -notmatch '@vsource\[i\]\s*=\s*([-+0-9.eE]+)') { throw "Corrente ausente: $value" }
    $current = [Math]::Abs([double]::Parse($Matches[1], [Globalization.CultureInfo]::InvariantCulture))
    if ($current -lt $case.Min -or $current -gt $case.Max) { throw "Corrente fora do intervalo em $($case.Name): $current" }
    Write-Output "LED $($case.Name): $current A verificados."
}
$result = & (Join-Path $resolvedRoot 'bin/ngspice_con.exe') -b 'build/netlist-rc.cir' 2>&1
if ($LASTEXITCODE -ne 0) { throw "Falha ngspice (RC): $result" }
$value = $result -join "`n"
if ($value -match '(?im)^\s*error\b' -or $value -match '(?i)couldn.t be loaded') { throw "Erro do ngspice: $value" }
$rows = [regex]::Matches($value, '(?m)^\s*\d+\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s*$')
if ($rows.Count -lt 2) { throw 'A análise RC não retornou amostras.' }
$bestDistance = [double]::PositiveInfinity
foreach ($row in $rows) {
    $time = [double]::Parse($row.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $voltage = [double]::Parse($row.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    $distance = [Math]::Abs($time - 0.001)
    if ($distance -lt $bestDistance) { $bestDistance = $distance; $bestTime = $time; $bestVoltage = $voltage }
}
$expected = 5 * (1 - [Math]::Exp(-$bestTime / 0.001))
if ($bestDistance -gt 0.000011 -or [Math]::Abs($bestVoltage - $expected) -gt 0.01) { throw "Resposta RC inesperada: t=$bestTime V=$bestVoltage" }
Write-Output "RC da bancada: $bestVoltage V em $bestTime s verificados."
$result = & (Join-Path $resolvedRoot 'bin/ngspice_con.exe') -b 'build/netlist-rl.cir' 2>&1
if ($LASTEXITCODE -ne 0) { throw "Falha ngspice (RL): $result" }
$value = $result -join "`n"
if ($value -match '(?im)^\s*error\b' -or $value -match '(?i)couldn.t be loaded') { throw "Erro do ngspice: $value" }
$rows = [regex]::Matches($value, '(?m)^\s*\d+\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s*$')
if ($rows.Count -lt 2) { throw 'A análise RL não retornou amostras.' }
$bestDistance = [double]::PositiveInfinity
foreach ($row in $rows) {
    $time = [double]::Parse($row.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $voltage = [double]::Parse($row.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    $distance = [Math]::Abs($time - 0.00001)
    if ($distance -lt $bestDistance) { $bestDistance = $distance; $bestTime = $time; $bestVoltage = $voltage }
}
$expected = 5 * [Math]::Exp(-$bestTime / 0.00001)
if ($bestDistance -gt 0.00000011 -or [Math]::Abs($bestVoltage - $expected) -gt 0.01) { throw "Resposta RL inesperada: t=$bestTime V=$bestVoltage" }
Write-Output "RL da bancada: $bestVoltage V em $bestTime s verificados."
