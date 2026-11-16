param(
    [Parameter(Mandatory)][string]$Stage,
    [Parameter(Mandatory)][string]$Manifest,
    [Parameter(Mandatory)][string]$TestBuild,
    [Parameter(Mandatory)][string]$Work,
    [Parameter(Mandatory)][string]$Version
)
$ErrorActionPreference = 'Stop'
$productKey = 'Bancada-PackageTest-' + [guid]::NewGuid().ToString('N')
$registry = "HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/$productKey"
$destination = Join-Path $Work 'Installed app with spaces'
$installer = Join-Path $Work 'test-setup.exe'
& makensis "-DPAYLOAD=$Stage" "-DOUTPUT=$installer" "-DVERSION=$Version" "-DDELETE_MANIFEST=$Manifest" "-DPRODUCT_KEY=$productKey" packaging/Bancada.nsi | Select-Object -Last 4
if ($LASTEXITCODE -ne 0) { throw 'Cannot build isolated installer test' }
function Run-Installer($file, $arguments) {
    $process = Start-Process -FilePath $file -ArgumentList $arguments -WindowStyle Hidden -PassThru
    $null = $process.Handle
    if (!$process.WaitForExit(60000)) { Stop-Process -Id $process.Id; throw 'Installer test timed out' }
    $process.Refresh()
    if ($process.ExitCode -ne 0) { throw "Installer exited with $($process.ExitCode)" }
}
# A separate registry/shortcut name prevents a test from replacing a real installation.
try {
    Run-Installer $installer @('/S', "/D=$destination")
    if (!(Test-Path $registry) -or !(Test-Path "$destination/installed.mode") -or (Test-Path "$destination/portable.mode")) {
        throw 'Installed mode or per-user registration missing'
    }
    & "$PSScriptRoot/test-package.ps1" -Package $destination -TestBuild $TestBuild
    # Simulate a file added by a person. The uninstaller must remove only shipped files.
    $sentinel = Join-Path $destination 'my-project.elec'
    'keep this project' | Set-Content $sentinel
    Run-Installer (Join-Path $destination 'Uninstall.exe') @('/S', "_?=$destination")
    if ((Test-Path $registry) -or (Test-Path "$destination/bancada.exe") -or !(Test-Path $sentinel)) {
        throw 'Uninstall did not preserve unknown files or remove application registration'
    }
    Write-Output 'Installer: per-user install, relocated simulation, uninstall and preservation of unknown files passed.'
} finally {
    if (Test-Path $registry) { Remove-Item -LiteralPath $registry }
}
