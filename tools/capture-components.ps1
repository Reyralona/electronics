param(
    [string]$Executable = 'build/bancada-next.exe',
    [string]$Destination = 'build/component-review/before',
    [switch]$IncludeChips
)
$ErrorActionPreference = 'Stop'
$binary = (Resolve-Path -LiteralPath $Executable).Path
$root = [IO.Path]::GetFullPath((Get-Location).Path)
$output = [IO.Path]::GetFullPath((Join-Path $root $Destination))
if (!$output.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'As capturas devem ficar dentro da pasta do projeto.'
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
$components = & $binary --list-components
if ($LASTEXITCODE -ne 0) { throw 'Nao foi possivel consultar o registro de componentes.' }
$rows = @()
foreach ($component in $components) {
    $record = $component -split '\|'
    if ($record.Count -ne 3) { throw "Registro invalido: $component" }
    $id = $record[0]
    $flags = [int]$record[2]
    if (!$IncludeChips -and ($flags -band 16)) { continue }
    foreach ($theme in @('light', 'dark')) {
        $argument = "--capture-component=$id"
        if ($theme -eq 'dark') { $argument = "--capture-component-dark=$id" }
        $capture = Start-Process -FilePath $binary -ArgumentList $argument -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardError 'build/component-capture.err'
        $null = $capture.Handle
        if (!$capture.WaitForExit(10000)) {
            throw "Captura pendente: $id/$theme (PID $($capture.Id)); inspecione o processo antes de repetir."
        }
        $capture.Refresh()
        if ($capture.ExitCode -ne 0) {
            Get-Content 'build/component-capture.err'
            throw "Falha na captura: $id/$theme"
        }
        $target = Join-Path $output "$id-$theme.bmp"
        Copy-Item -LiteralPath 'build/bancada-capture.bmp' -Destination $target
        $rows += [PSCustomObject]@{id=$id; name=$record[1]; theme=$theme; image="$id-$theme.bmp"}
    }
}
$rows | Export-Csv -LiteralPath (Join-Path $output 'index.csv') -NoTypeInformation -Encoding UTF8
$cards = foreach ($row in $rows) {
    $name = [Net.WebUtility]::HtmlEncode($row.name)
    "<figure><img loading='lazy' src='$($row.image)' alt='$name'><figcaption>$name / $($row.theme)</figcaption></figure>"
}
$html = "<!doctype html><meta charset='utf-8'><title>Bancada - revisao de componentes</title><style>body{background:#18222e;color:#e7eef4;font:16px Segoe UI,sans-serif;margin:24px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(500px,1fr));gap:20px}figure{margin:0}img{width:100%;display:block}figcaption{padding:8px}</style><h1>Close-ups de componentes</h1><main>$($cards -join [Environment]::NewLine)</main>"
[IO.File]::WriteAllText((Join-Path $output 'index.html'), $html, [Text.UTF8Encoding]::new($false))
Write-Output "Capturas geradas: $($rows.Count). Galeria: $output/index.html"
