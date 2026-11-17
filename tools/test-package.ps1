param([Parameter(Mandatory)][string]$Package, [Parameter(Mandatory)][string]$TestBuild)
$ErrorActionPreference = 'Stop'
$packagePath = (Resolve-Path -LiteralPath $Package).Path
$testPath = (Resolve-Path -LiteralPath $TestBuild).Path
$solver = Join-Path $packagePath 'spice-local/ucrt64/bin/ngspice_con.exe'
$originalPath = $env:Path
$work = Join-Path $testPath ('package-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $work | Out-Null
try {
    $env:Path = "$env:SystemRoot/System32;$env:SystemRoot"
    Push-Location $work
    try {
        foreach ($test in 'test_spice_process.exe', 'test_workbench_examples.exe', 'test_spice_session.exe', 'test_spice_live.exe', 'test_terminal_contacts.exe', 'test_cockcroft_slot.exe') {
            $temporaryTest = Join-Path $packagePath $test
            if (Test-Path -LiteralPath $temporaryTest) { throw "Test file already exists: $temporaryTest" }
            Copy-Item -LiteralPath (Join-Path $testPath $test) -Destination $temporaryTest
            try {
                if ($test -eq 'test_cockcroft_slot.exe') {
                    $fixture = Join-Path (Split-Path $PSScriptRoot -Parent) 'tests/fixtures/cockcroft-walton.elec'
                    & $temporaryTest $fixture $solver
                } else {
                    & $temporaryTest $solver
                }
                if ($LASTEXITCODE -ne 0) { throw "Relocated package failed: $test ($LASTEXITCODE)" }
            } finally { Remove-Item -LiteralPath $temporaryTest }
        }
        $screenshot = Join-Path $work 'package.bmp'
        $app = Start-Process -FilePath (Join-Path $packagePath 'bancada.exe') -ArgumentList '--capture-example2', "--capture-output=$screenshot" -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $work 'capture.err')
        $null = $app.Handle
        if (!$app.WaitForExit(20000)) { Stop-Process -Id $app.Id; throw 'Package screenshot timed out' }
        $app.Refresh()
        if ($app.ExitCode -ne 0 -or !(Test-Path $screenshot)) { throw 'Package executable failed to render' }
        $directoryOutput = Join-Path $work 'data-directory.txt'
        $probe = Start-Process -FilePath (Join-Path $packagePath 'bancada.exe') -ArgumentList '--print-data-directory' -WindowStyle Hidden -PassThru -RedirectStandardOutput $directoryOutput
        $null = $probe.Handle
        if (!$probe.WaitForExit(10000)) { Stop-Process -Id $probe.Id; throw 'Data directory probe timed out' }
        $probe.Refresh()
        $expected = $packagePath
        if (Test-Path (Join-Path $packagePath 'installed.mode')) {
            $expected = Join-Path $env:APPDATA 'Bancada/Bancada'
        }
        $actual = (Get-Content $directoryOutput -Raw -Encoding UTF8).Trim()
        if ($probe.ExitCode -ne 0 -or [IO.Path]::GetFullPath($actual) -ne [IO.Path]::GetFullPath($expected)) {
            throw "Wrong data directory: $actual (expected $expected)"
        }
        Write-Output 'Relocated package: DC, digital transient, persistent sessions, live memory and UI passed with minimal PATH.'
    } finally { Pop-Location }
} finally { $env:Path = $originalPath }
