param([string]$Tag, [string]$Artifacts = 'build/dist', [switch]$ResumeDraft)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
Push-Location $repo
try {
    if (!$Tag) { $Tag = 'v' + (Get-Content VERSION -Raw).Trim() }
    if ($Tag -notmatch '^v\d+\.\d+\.\d+$') { throw 'Expected a version tag such as v0.36.0.' }
    $version = $Tag.Substring(1)
    $tagVersion = & git show "${Tag}:VERSION"
    if ($LASTEXITCODE -ne 0 -or $tagVersion.Trim() -ne $version) { throw 'Tag and VERSION do not match.' }
    $directory = (Resolve-Path -LiteralPath $Artifacts).Path
    $names = @("Bancada-$version-windows-x64-setup.exe", "Bancada-$version-windows-x64.zip", "Bancada-$version-sources.zip")
    $checksumsName = "Bancada-$version-SHA256SUMS.txt"
    $checksums = Join-Path $directory $checksumsName
    $seen = @{}
    foreach ($line in Get-Content -LiteralPath $checksums) {
        if ($line -notmatch '^([a-fA-F0-9]{64})  (.+)$') { throw 'Invalid checksum line.' }
        $hash = $Matches[1]
        $name = $Matches[2]
        if ($name -notin $names -or $seen.ContainsKey($name)) { throw "Unexpected or duplicated asset: $name" }
        if ((Get-FileHash (Join-Path $directory $name) -Algorithm SHA256).Hash -ne $hash) { throw "Hash mismatch: $name" }
        $seen[$name] = $hash
    }
    if ($seen.Count -ne $names.Count) { throw 'Installer, ZIP and corresponding sources are required.' }
    # Confirm the actual binary provenance against the selected tag, allowing documentation-only changes.
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [IO.Compression.ZipFile]::OpenRead((Join-Path $directory "Bancada-$version-windows-x64.zip"))
    try {
        $entry = @($zip.Entries | Where-Object { $_.FullName -match '(^|/)build-info.json$' })
        if ($entry.Count -ne 1) { throw 'Missing or ambiguous build provenance.' }
        $reader = [IO.StreamReader]::new($entry[0].Open())
        try { $build = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
    } finally { $zip.Dispose() }
    if ($build.version -ne $version -or $build.commit -notmatch '^[a-f0-9]{40}$') { throw 'Invalid build provenance.' }
    & git merge-base --is-ancestor $build.commit $Tag
    if ($LASTEXITCODE -ne 0) { throw 'The build commit is not part of the selected tag.' }
    & git diff --quiet $build.commit $Tag -- src CMakeLists.txt packaging build.ps1 Empacotar.cmd tools/package-windows.ps1 tools/test-package.ps1 tools/test-installer.ps1 VERSION
    if ($LASTEXITCODE -ne 0) { throw 'The tagged application or packaging differs from the tested build.' }
    $changelog = (& git show "${Tag}:CHANGELOG.md") -join "`n"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read the tagged changelog.' }
    $section = [regex]::Match($changelog, '(?ms)^## ' + [regex]::Escape($version) + '\s*\n(.*?)(?=^## |\z)')
    if (!$section.Success) { throw 'Missing changelog section for this version.' }
    $notes = Join-Path $repo "build/release-$version.md"
    $text = @"
Versão experimental para Windows 10/11 x64, com interface em português e inglês.

## Downloads

- **Instalador:** arquivo ``-setup.exe``; instala só para você, sem administrador.
- **Portátil:** extraia o ZIP ``windows-x64`` inteiro e abra ``bancada.exe``.
- **Fontes:** ``sources.zip`` inclui o projeto e os fontes/patches correspondentes das dependências.
- **Integridade:** confira os downloads com ``SHA256SUMS.txt``.

Não é preciso instalar MSYS2, GCC ou ngspice para usar estes pacotes.
Na instalação, projetos ficam em ``%APPDATA%/Bancada/Bancada`` e sobrevivem à desinstalação.
No ZIP, ficam ao lado do executável. Não há migração automática de projetos antigos.

## Alterações

$($section.Groups[1].Value.Trim())

## Estado de validação

Artefatos sem assinatura digital. A suíte local, o pacote movido para uma pasta com espaços,
as simulações com PATH mínimo e a instalação/desinstalação foram verificados.
Ainda falta validação numa VM Windows limpa. Os modelos elétricos são aproximações didáticas.

Commit dos binários: ``$($build.commit)``. O arquivo ``build-info.json`` identifica as ferramentas usadas.
"@
    [IO.File]::WriteAllText($notes, $text, [Text.UTF8Encoding]::new($false))
    $assets = @($names + $checksumsName | ForEach-Object { Join-Path $directory $_ })
    # Keep incomplete uploads private. Existing releases fail instead of replacing published files.
    if ($ResumeDraft) {
        $existing = (& gh release view $Tag --json isDraft) | ConvertFrom-Json
        if ($LASTEXITCODE -ne 0 -or !$existing.isDraft) { throw 'Only an existing draft can be resumed.' }
        & gh release edit $Tag --notes-file $notes
    } else {
        & gh release create $Tag @assets --verify-tag --draft --prerelease --title "Bancada $version" --notes-file $notes
    }
    if ($LASTEXITCODE -ne 0) { throw 'Release creation failed. Any partial draft was preserved for inspection.' }
    # GitHub's releases/tags endpoint does not expose drafts; use the authenticated CLI lookup.
    $apiUrl = & gh release view $Tag --json apiUrl --jq .apiUrl
    if ($LASTEXITCODE -ne 0) { throw 'Cannot locate the draft for asset verification.' }
    $remote = (& gh api $apiUrl) | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0 -or $remote.assets.Count -ne $assets.Count) { throw 'Uploaded assets are incomplete; release remains a draft.' }
    foreach ($asset in $remote.assets) {
        $local = Join-Path $directory $asset.name
        if ($asset.name -notin ($names + $checksumsName)) { throw 'Unexpected remote asset; release remains a draft.' }
        if ($asset.size -ne (Get-Item -LiteralPath $local).Length) { throw 'Remote asset size mismatch; release remains a draft.' }
        if ($asset.digest -and $asset.digest -ne ('sha256:' + (Get-FileHash $local -Algorithm SHA256).Hash.ToLower())) {
            throw 'Remote asset hash mismatch; release remains a draft.'
        }
    }
    & gh release edit $Tag --draft=false --prerelease --latest=false
    if ($LASTEXITCODE -ne 0) { throw 'Publishing failed; verified draft was preserved.' }
    & gh release view $Tag --json url,isDraft,isPrerelease
} finally { Pop-Location }
