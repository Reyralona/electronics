param([string]$Toolchain = 'C:/msys64/ucrt64', [switch]$SkipTests)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
$previousPath = $env:Path
try {
    $env:Path = "$Toolchain/bin;$env:Path"
    $tar = 'C:/msys64/usr/bin/bsdtar.exe'
    foreach ($command in 'cmake', 'ninja', 'gcc', 'objdump', 'makensis') {
        if (!(Get-Command $command -ErrorAction SilentlyContinue)) { throw "Missing tool: $command" }
    }
    if (!(Test-Path $tar)) { throw "Missing tool: $tar" }
    $version = (Get-Content VERSION -Raw).Trim()
    $lock = Get-Content packaging/dependencies.lock.json -Raw | ConvertFrom-Json
    $run = Join-Path $repo ('build/package-' + [guid]::NewGuid().ToString('N'))
    $runtime = Join-Path $run 'runtime'
    $payload = Join-Path $run 'payload'
    $downloads = Join-Path $repo 'build/package-downloads'
    $output = Join-Path $repo 'build/dist'
    New-Item -ItemType Directory -Force $runtime, $payload, $downloads, $output | Out-Null

    function Get-VerifiedFile($url, $name, $hash) {
        $path = Join-Path $downloads $name
        if (!(Test-Path -LiteralPath $path)) {
            $partial = "$path.partial"
            Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $partial
            if ((Get-FileHash $partial -Algorithm SHA256).Hash -ne $hash) { throw "Hash mismatch: $name" }
            Move-Item -LiteralPath $partial -Destination $path
        }
        if ((Get-FileHash $path -Algorithm SHA256).Hash -ne $hash) { throw "Hash mismatch: $name" }
        return $path
    }

    $sourceDir = Join-Path $run 'sources'
    New-Item -ItemType Directory -Force $sourceDir | Out-Null
    foreach ($package in $lock.packages) {
        Write-Output "Verified dependency: $($package.file)"
        $archive = Get-VerifiedFile $package.url $package.file $package.sha256
        & $tar -xf $archive -C $runtime
        if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $archive" }
        if ($package.sourceSha256) {
            $source = Get-VerifiedFile $package.sourceUrl $package.sourceFile $package.sourceSha256
            Copy-Item -LiteralPath $source -Destination $sourceDir
        }
    }
    Copy-Item packaging/dependencies.lock.json (Join-Path $payload 'dependencies.lock.json')
    Copy-Item LICENSE (Join-Path $payload 'LICENSE')
    Copy-Item README.md (Join-Path $payload 'README.md')
    Copy-Item docs (Join-Path $payload 'docs') -Recurse
    Copy-Item (Join-Path $runtime 'ucrt64/share/licenses') (Join-Path $payload 'licenses') -Recurse
    # FFTW's GPL text is in its matching source archive rather than share/licenses.
    $sourceWork = Join-Path $run 'source-work'
    New-Item -ItemType Directory -Force $sourceWork | Out-Null
    $fftw = $lock.packages | Where-Object { $_.file -match '-fftw-' }
    & $tar -xf (Join-Path $sourceDir $fftw.sourceFile) -C $sourceWork
    if ($LASTEXITCODE -ne 0) { throw 'Cannot unpack FFTW source license' }
    $upstream = Get-ChildItem $sourceWork -Recurse -Filter 'fftw-*.tar.gz' | Select-Object -First 1
    & $tar -xf $upstream.FullName -C $sourceWork
    if ($LASTEXITCODE -ne 0) { throw 'Cannot unpack FFTW license' }
    Copy-Item (Join-Path $sourceWork 'mingw-w64-fftw/fftw-3.3.10/COPYING') (Join-Path $payload 'licenses/FFTW-COPYING') -ErrorAction SilentlyContinue
    if (!(Test-Path (Join-Path $payload 'licenses/FFTW-COPYING'))) {
        Copy-Item (Join-Path $sourceWork 'fftw-3.3.10/COPYING') (Join-Path $payload 'licenses/FFTW-COPYING')
    }

    $solverBin = Join-Path $payload 'spice-local/ucrt64/bin'
    $modelDir = Join-Path $payload 'spice-local/ucrt64/lib/ngspice'
    $scripts = Join-Path $payload 'spice-local/ucrt64/share/ngspice/scripts'
    New-Item -ItemType Directory -Force $solverBin, $modelDir, $scripts | Out-Null
    $runtimeBin = Join-Path $runtime 'ucrt64/bin'
    function Copy-BinaryClosure([string]$source, [string]$destination) {
        $target = Join-Path $destination (Split-Path $source -Leaf)
        if (Test-Path -LiteralPath $target) { return }
        Copy-Item -LiteralPath $source -Destination $target
        $imports = & objdump -p $source
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect binary: $source" }
        foreach ($line in $imports) {
            if ($line -notmatch 'DLL Name:\s*(\S+)') { continue }
            $name = $Matches[1]
            $dependency = Join-Path $runtimeBin $name
            if (Test-Path -LiteralPath $dependency) {
                Copy-BinaryClosure $dependency $destination
            } elseif ($name -notmatch '^(api-ms-|ext-ms-)' -and !(Test-Path -LiteralPath "$env:SystemRoot/System32/$name")) {
                throw "Unresolved DLL: $name imported by $source"
            }
        }
    }
    Copy-BinaryClosure (Join-Path $runtimeBin 'SDL3.dll') $payload
    Copy-BinaryClosure (Join-Path $runtimeBin 'ngspice_con.exe') $solverBin
    Copy-BinaryClosure (Join-Path $runtimeBin 'libngspice-0.dll') $solverBin
    $model = Join-Path $runtime 'ucrt64/lib/ngspice/digital.cm'
    Copy-BinaryClosure $model $modelDir
    # Model imports also need to resolve next to libngspice, not via the development PATH.
    Get-ChildItem $modelDir -Filter '*.dll' | Copy-Item -Destination $solverBin
    'codemodel ../lib/ngspice/digital.cm' | Set-Content -Encoding ASCII (Join-Path $scripts 'spinit')
    '' | Set-Content -Encoding ASCII (Join-Path $payload 'portable.mode')
    Copy-Item packaging/LEIA-ME.txt (Join-Path $payload 'LEIA-ME.txt')

    $cmakeBuild = Join-Path $repo 'build/cmake-release'
    & cmake -S . -B $cmakeBuild -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DBANCADA_PAYLOAD=$payload" "-DSDL3_DIR=$runtime/ucrt64/lib/cmake/SDL3" '-DBUILD_TESTING=ON'
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
    & cmake --build $cmakeBuild --parallel
    if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }
    Get-ChildItem $payload -Filter '*.dll' | Copy-Item -Destination $cmakeBuild
    # Use an unmarked staging directory so package tests cannot touch real user data.
    $stage = Join-Path $run 'Bancada relocated with spaces'
    & cmake --install $cmakeBuild --prefix $stage
    if ($LASTEXITCODE -ne 0) { throw 'CMake install failed' }
    foreach ($exe in 'bancada.exe', 'spice-live-host.exe') {
        $binary = Join-Path $cmakeBuild $exe
        # Inspect compiler runtime imports too, copying only locked dependencies.
        $scratch = Join-Path $run $exe.Replace('.exe', '')
        New-Item -ItemType Directory -Force $scratch | Out-Null
        Copy-BinaryClosure $binary $scratch
        Get-ChildItem $scratch -Filter '*.dll' | Copy-Item -Destination $payload
    }
    & cmake --install $cmakeBuild --prefix $stage
    if ($LASTEXITCODE -ne 0) { throw 'Final staging failed' }
    if (!$SkipTests) {
        & ctest --test-dir $cmakeBuild --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'CTest failed' }
        & "$PSScriptRoot/test-package.ps1" -Package $stage -TestBuild $cmakeBuild
    }
    # Ship the exact application source alongside dependency sources.
    & git archive --format=zip "--output=$sourceDir/Bancada-source.zip" HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot create corresponding application source' }
    if ((git status --porcelain --untracked-files=no)) {
        throw 'Commit the reviewed source before producing distributable artifacts.'
    }
    Compress-Archive -Path "$sourceDir/*" -DestinationPath (Join-Path $output "Bancada-$version-sources.zip") -Force
    & cmake --install $cmakeBuild --prefix $stage
    if ($LASTEXITCODE -ne 0) { throw 'Source staging failed' }
    # Delete only listed shipped files during uninstall; preserve unknown files and all saved projects.
    $manifest = Join-Path $run 'uninstall-files.nsh'
    $deleteLines = @(Get-ChildItem $stage -File -Recurse | ForEach-Object {
        $relative = $_.FullName.Substring($stage.Length + 1)
        'Delete "$INSTDIR\' + $relative + '"'
    })
    $deleteLines += @(Get-ChildItem $stage -Directory -Recurse | Sort-Object { $_.FullName.Length } -Descending | ForEach-Object {
        'RMDir "$INSTDIR\' + $_.FullName.Substring($stage.Length + 1) + '"'
    })
    $deleteLines | Set-Content -Encoding UTF8 $manifest
    Push-Location $output
    try {
        & cpack --config "$cmakeBuild/CPackConfig.cmake" -G ZIP
        if ($LASTEXITCODE -ne 0) { throw 'ZIP packaging failed' }
    } finally { Pop-Location }
    $installer = Join-Path $output "Bancada-$version-windows-x64-setup.exe"
    & makensis "-DPAYLOAD=$stage" "-DOUTPUT=$installer" "-DVERSION=$version" "-DDELETE_MANIFEST=$manifest" packaging/Bancada.nsi
    if ($LASTEXITCODE -ne 0) { throw 'NSIS packaging failed' }
    if (!$SkipTests) {
        & "$PSScriptRoot/test-installer.ps1" -Stage $stage -Manifest $manifest -TestBuild $cmakeBuild -Work $run -Version $version
    }
    Get-ChildItem $output -File | Where-Object { $_.Name -in @("Bancada-$version-windows-x64.zip", "Bancada-$version-windows-x64-setup.exe", "Bancada-$version-sources.zip") } | ForEach-Object {
        (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower() + '  ' + $_.Name
    } | Set-Content -Encoding ASCII (Join-Path $output "Bancada-$version-SHA256SUMS.txt")
    Write-Output "Unsigned artifacts: $output"
} finally {
    $env:Path = $previousPath
    Pop-Location
}
