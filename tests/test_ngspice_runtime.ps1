$ErrorActionPreference = 'Stop'
$check = Join-Path $PSScriptRoot '../tools/test-ngspice.ps1'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
$originalPath = $env:Path
try {
    # A fresh checkout and an incomplete installation must explain how to recover.
    foreach ($missing in 'bin/ngspice_con.exe', 'bin/libngspice-0.dll', 'lib/ngspice/digital.cm') {
        $failure = $null
        try { & $check -Root $fixture }
        catch { $failure = $_.Exception.Message }
        if (!$failure -or !$failure.Contains($missing) -or !$failure.Contains('./tools/setup-ngspice.ps1')) {
            throw "Missing recovery instructions for ${missing}: $failure"
        }
        if ($env:Path -ne $originalPath) { throw 'Runtime check changed PATH.' }
        $file = Join-Path $fixture $missing
        New-Item -ItemType Directory -Force (Split-Path $file -Parent) | Out-Null
        New-Item -ItemType File $file | Out-Null
    }
    # Existing but unusable files must fail too, and restore the caller's PATH.
    $failure = $null
    try { & $check -Root $fixture }
    catch { $failure = $_.Exception.Message }
    if (!$failure -or !$failure.Contains('./tools/setup-ngspice.ps1')) {
        throw "Invalid runtime was accepted: $failure"
    }
    if ($env:Path -ne $originalPath) { throw 'Failed solver launch changed PATH.' }
} finally {
    # Remove only the files and directories created by this test.
    if (Test-Path $fixture) {
        Get-ChildItem -LiteralPath $fixture -File -Recurse | Remove-Item
        Remove-Item -LiteralPath (Join-Path $fixture 'lib/ngspice'), (Join-Path $fixture 'lib'), (Join-Path $fixture 'bin'), $fixture -ErrorAction SilentlyContinue
    }
}
Write-Output 'Missing and incomplete ngspice runtimes report recovery instructions.'
