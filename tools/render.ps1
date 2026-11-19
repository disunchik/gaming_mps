# Render recorded UART cells, not a desktop screenshot. No application automation.
param([string]$Name = 'main')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$directory = Join-Path (Split-Path $PSScriptRoot -Parent) 'artifacts\screens'
$grid = Get-Content (Join-Path $directory "$Name.json") -Raw -Encoding UTF8 | ConvertFrom-Json
$rows = $grid.Count
$columns = $grid[0].Count
$bitmap = New-Object Drawing.Bitmap(($columns * 12), ($rows * 24))
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::ClearTypeGridFit
$font = New-Object Drawing.Font('Consolas', 20, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
$format = [Drawing.StringFormat]::GenericTypographic
$brushes = @{}
function Brush([int]$index) {
    if (!$brushes.ContainsKey($index)) {
        if ($index -ge 232) {
            $v = 8 + 10 * ($index - 232)
            $color = [Drawing.Color]::FromArgb($v, $v, $v)
        } elseif ($index -ge 16) {
            $n = $index - 16; $levels = @(0, 95, 135, 175, 215, 255)
            $color = [Drawing.Color]::FromArgb($levels[[int][math]::Floor($n / 36)], $levels[[int][math]::Floor($n / 6) % 6], $levels[$n % 6])
        } else { $color = [Drawing.Color]::White }
        $brushes[$index] = New-Object Drawing.SolidBrush($color)
    }
    return $brushes[$index]
}
try {
    for ($y = 0; $y -lt $rows; $y++) {
        for ($x = 0; $x -lt $columns; $x++) {
            $cell = $grid[$y][$x]
            $graphics.FillRectangle((Brush $cell[2]), $x * 12, $y * 24, 12, 24)
            if ($cell[0] -ne ' ') {
                $graphics.DrawString([string]$cell[0], $font, (Brush $cell[1]), [single]($x * 12), [single]($y * 24), $format)
            }
        }
    }
    $bitmap.Save((Join-Path $directory "$Name.png"), [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $graphics.Dispose(); $font.Dispose(); $bitmap.Dispose()
    foreach ($brush in $brushes.Values) { $brush.Dispose() }
}
