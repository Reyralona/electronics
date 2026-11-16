param(
    [string]$Directory = 'build/component-review/final',
    [ValidateSet('light', 'dark')][string]$Theme = 'light'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = (Resolve-Path -LiteralPath $Directory).Path
$rows = @(Import-Csv -LiteralPath (Join-Path $root 'index.csv') | Where-Object { $_.theme -eq $Theme })
$columns = 4
$tileWidth = 480
$tileHeight = 300
$sheet = [Drawing.Bitmap]::new($columns * $tileWidth, [int][Math]::Ceiling($rows.Count / $columns) * $tileHeight)
$graphics = [Drawing.Graphics]::FromImage($sheet)
$font = [Drawing.Font]::new('Segoe UI', 13)
try {
    $background = [Drawing.Color]::White
    $caption = [Drawing.Brushes]::Black
    $sheetName = 'contact-sheet.png'
    if ($Theme -eq 'dark') {
        $background = [Drawing.Color]::FromArgb(24, 34, 46)
        $caption = [Drawing.Brushes]::White
        $sheetName = 'contact-sheet-dark.png'
    }
    $graphics.Clear($background)
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    for ($index = 0; $index -lt $rows.Count; $index++) {
        $x = ($index % $columns) * $tileWidth
        $y = [int][Math]::Floor($index / $columns) * $tileHeight
        $source = [Drawing.Bitmap]::FromFile((Join-Path $root $rows[$index].image))
        try {
            $target = [Drawing.Rectangle]::new($x, $y + 28, 480, 267)
            $graphics.DrawImage($source, $target, 220, 70, 1060, 590, [Drawing.GraphicsUnit]::Pixel)
            $graphics.DrawString($rows[$index].name, $font, $caption, [float]($x + 12), [float]($y + 3))
        } finally { $source.Dispose() }
    }
    $sheet.Save((Join-Path $root $sheetName), [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $font.Dispose()
    $graphics.Dispose()
    $sheet.Dispose()
}
Write-Output "Contact sheet: $root/$sheetName"
