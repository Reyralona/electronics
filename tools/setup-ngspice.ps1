param([string]$Destination = 'build/spice-local')
$ErrorActionPreference = 'Stop'
$pacman = 'C:\msys64\usr\bin\pacman.exe'
$tar = 'C:\msys64\usr\bin\bsdtar.exe'
if (!(Test-Path -LiteralPath $pacman) -or !(Test-Path -LiteralPath $tar)) { throw 'MSYS2 e pacman são necessários em C:\msys64.' }
$targetPath = [IO.Path]::GetFullPath((Join-Path (Get-Location) $Destination))
if ($targetPath -match '\s') { throw 'A instalação experimental do ngspice ainda não suporta caminhos com espaços.' }
if (Test-Path -LiteralPath $targetPath) { throw "Destino já existe e será preservado: $targetPath" }
$stagingPath = "$targetPath.staging-$PID"
$cachePath = "$targetPath.cache-$PID"
if ((Test-Path -LiteralPath $stagingPath) -or (Test-Path -LiteralPath $cachePath)) { throw 'Diretório temporário já existe; escolha outro destino.' }
New-Item -ItemType Directory -Path $stagingPath, $cachePath -Force | Out-Null
# Explicit targets are downloaded even when these dependencies are installed in MSYS2 already.
$packages = @('mingw-w64-ucrt-x86_64-ngspice', 'mingw-w64-ucrt-x86_64-gcc-libs',
              'mingw-w64-ucrt-x86_64-libwinpthread')
& $pacman -Sw --noconfirm --cachedir $cachePath @packages
if ($LASTEXITCODE -ne 0) { throw 'Download ou verificação de integridade do pacman falhou.' }
$archives = @(Get-ChildItem -LiteralPath $cachePath -Filter '*.pkg.tar.zst')
if (!($archives | Where-Object { $_.Name -match '^mingw-w64-ucrt-x86_64-ngspice-' })) { throw 'O pacote do ngspice não foi baixado.' }
foreach ($archive in $archives) {
    & $tar -xf $archive.FullName -C $stagingPath
    if ($LASTEXITCODE -ne 0) { throw "Falha ao extrair $($archive.Name)." }
}
foreach ($dll in 'libgcc_s_seh-1.dll', 'libgomp-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll') {
    if (!(Test-Path -LiteralPath (Join-Path $stagingPath "ucrt64/bin/$dll"))) {
        throw "Runtime incompleto: $dll ausente; instalacao nao publicada."
    }
}
$initPath = Join-Path $stagingPath 'ucrt64/share/ngspice/scripts/spinit'
if (!(Test-Path -LiteralPath $initPath)) { throw 'spinit não encontrado no pacote.' }
$init = [IO.File]::ReadAllText($initPath)
if (!$init.Contains('/ucrt64/lib/ngspice')) { throw 'Formato inesperado do spinit; instalação não publicada.' }
$runtimePath = (Join-Path $targetPath 'ucrt64/lib/ngspice').Replace('\','/')
[IO.File]::WriteAllText($initPath, $init.Replace('/ucrt64/lib/ngspice', $runtimePath), [Text.Encoding]::ASCII)
if (Test-Path -LiteralPath $targetPath) { throw 'Destino criado durante a preparação; instalação não publicada.' }
Move-Item -LiteralPath $stagingPath -Destination $targetPath
Write-Output "Runtime de desenvolvimento instalado em $targetPath. Cache preservado em $cachePath."
Write-Output "Valide com: ./tools/test-ngspice.ps1 -Root '$Destination/ucrt64'"
