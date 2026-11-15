param([string]$Root = 'build/spice-local/ucrt64')
$ErrorActionPreference = 'Stop'
$solver = (Resolve-Path (Join-Path $Root 'bin/ngspice_con.exe')).Path
$originalPath = $env:Path
try {
    $env:Path = "$env:SystemRoot\System32;$env:SystemRoot"
    foreach ($test in 'test_sim_auto.exe', 'test_workbench_examples.exe') {
        $testPath = (Resolve-Path (Join-Path 'build' $test)).Path
        & $testPath $solver
        if ($LASTEXITCODE -ne 0) { throw "$test falhou com PATH minimo: $LASTEXITCODE" }
    }
    Write-Output 'Runtime isolado: DC, transientes e exemplos passaram sem MSYS2 no PATH.'
} finally {
    $env:Path = $originalPath
}
