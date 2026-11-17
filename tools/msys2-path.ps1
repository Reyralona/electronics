# Finds the MSYS2 UCRT64 toolchain for this session, without depending on a terminal-specific PATH.
# Order: tools already on PATH; MSYS2_ROOT; the MSYS2 installer's default folder.
$Msys2DefaultRoot = 'C:\msys64'

function Get-Msys2Root {
    if ($env:MSYS2_ROOT) { return $env:MSYS2_ROOT }
    return $Msys2DefaultRoot
}

function Add-Msys2ToPath {
    if (Get-Command gcc.exe -ErrorAction SilentlyContinue) { return }
    $root = Get-Msys2Root
    $compilerDir = Join-Path $root 'ucrt64\bin'
    if (!(Test-Path (Join-Path $compilerDir 'gcc.exe'))) {
        throw "gcc.exe não está no PATH nem em $compilerDir. Instale o MSYS2 (docs/COMPILAR.md) ou defina MSYS2_ROOT com a pasta da instalação."
    }
    $env:Path = "$compilerDir;$(Join-Path $root 'usr\bin');$env:Path"
}
