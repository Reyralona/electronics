param([string]$Root = 'build/spice-local/ucrt64')
$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $Root).Path
$env:Path = "$(Join-Path $resolvedRoot 'bin');$env:Path"
$template = Get-Content -Raw 'tests/spice/xspice_and.cir'
$cases = @(
    @{ Name='and'; Values=@(0,0,0,5,2.5); Led=$true },
    @{ Name='nand'; Values=@(5,5,5,0,2.5); Led=$false },
    @{ Name='or'; Values=@(0,5,5,5,5); Led=$true },
    @{ Name='xor'; Values=@(0,5,5,0,2.5); Led=$false }
)
New-Item -ItemType Directory -Force build | Out-Null
foreach ($case in $cases) {
    $path = "build/xspice-$($case.Name).cir"
    $template.Replace('d_and(', "d_$($case.Name)(") | Set-Content -Encoding ascii $path
    $result = & (Join-Path $resolvedRoot 'bin/ngspice_con.exe') -b $path 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Falha ngspice ($($case.Name)): $result" }
    $output = $result -join "`n"
    if ($output -match '(?i)error|warning|failed|couldn.t be loaded|singular matrix') { throw "Resultado invalido ($($case.Name)): $output" }
    $names = @('case00','case10','case01','case11','undefined')
    for ($i=0; $i -lt $names.Count; $i++) {
        if ($output -notmatch "(?m)^$($names[$i])\s*=\s*([-+0-9.eE]+)") { throw "Amostra ausente $($names[$i]): $output" }
        $value = [double]::Parse($Matches[1],[Globalization.CultureInfo]::InvariantCulture)
        if ([Math]::Abs($value-$case.Values[$i]) -gt 0.01) { throw "Resultado $($case.Name) $($names[$i]): $value" }
    }
    if ($output -notmatch '(?m)^led_current\s*=\s*([-+0-9.eE]+)') { throw "Corrente do LED ausente: $output" }
    $current = [double]::Parse($Matches[1],[Globalization.CultureInfo]::InvariantCulture)
    if (($case.Led -and ($current -lt .003 -or $current -gt .004)) -or (!$case.Led -and [Math]::Abs($current) -gt 1e-8)) { throw "Corrente incorreta em $($case.Name): $current" }
    Write-Output "Porta $($case.Name): tabela, estado indefinido e carga LED verificados."
}
