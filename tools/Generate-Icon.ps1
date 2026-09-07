param(
    [string]$OutputPath = (Join-Path $PSScriptRoot '..\assets\touchscreen-toggle.ico'),
    [string]$PreviewPath = (Join-Path $PSScriptRoot '..\assets\touchscreen-toggle-preview.png')
)

Add-Type -AssemblyName System.Drawing

function New-RoundedRectanglePath {
    param([float]$X, [float]$Y, [float]$Width, [float]$Height, [float]$Radius)
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $diameter = $Radius * 2
    $path.AddArc($X, $Y, $diameter, $diameter, 180, 90)
    $path.AddArc($X + $Width - $diameter, $Y, $diameter, $diameter, 270, 90)
    $path.AddArc($X + $Width - $diameter, $Y + $Height - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($X, $Y + $Height - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

function New-IconPng {
    param([int]$Size)
    $bitmap = [System.Drawing.Bitmap]::new($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.Clear([System.Drawing.Color]::Transparent)

    $margin = [float]($Size * 0.04)
    $tile = New-RoundedRectanglePath $margin $margin ($Size - 2 * $margin) ($Size - 2 * $margin) ($Size * 0.22)
    $tileBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 37, 99, 235))
    $graphics.FillPath($tileBrush, $tile)

    $screen = New-RoundedRectanglePath ($Size * 0.19) ($Size * 0.17) ($Size * 0.62) ($Size * 0.66) ($Size * 0.075)
    $screenPen = [System.Drawing.Pen]::new([System.Drawing.Color]::White, [Math]::Max(1.5, $Size * 0.065))
    $screenPen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $graphics.DrawPath($screenPen, $screen)

    $touchBrush = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 52, 211, 153))
    $ringPen = [System.Drawing.Pen]::new([System.Drawing.Color]::White, [Math]::Max(1.0, $Size * 0.025))
    $dot = [float]($Size * 0.22)
    $dotX = [float]($Size * 0.60)
    $dotY = [float]($Size * 0.59)
    $graphics.FillEllipse($touchBrush, $dotX, $dotY, $dot, $dot)
    $graphics.DrawEllipse($ringPen, $dotX, $dotY, $dot, $dot)

    $stream = [System.IO.MemoryStream]::new()
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $stream.ToArray()

    if ($Size -eq 256) {
        $previewDirectory = Split-Path -Parent $PreviewPath
        [System.IO.Directory]::CreateDirectory($previewDirectory) | Out-Null
        $bitmap.Save($PreviewPath, [System.Drawing.Imaging.ImageFormat]::Png)
    }

    $stream.Dispose()
    $ringPen.Dispose()
    $touchBrush.Dispose()
    $screenPen.Dispose()
    $screen.Dispose()
    $tileBrush.Dispose()
    $tile.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
    return ,$bytes
}

$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
$images = foreach ($size in $sizes) {
    [PSCustomObject]@{ Size = $size; Bytes = (New-IconPng $size) }
}

$outputDirectory = Split-Path -Parent $OutputPath
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
$file = [System.IO.File]::Create($OutputPath)
$writer = [System.IO.BinaryWriter]::new($file)
$writer.Write([uint16]0)
$writer.Write([uint16]1)
$writer.Write([uint16]$images.Count)
$offset = 6 + (16 * $images.Count)
foreach ($image in $images) {
    $writer.Write([byte]($(if ($image.Size -eq 256) { 0 } else { $image.Size })))
    $writer.Write([byte]($(if ($image.Size -eq 256) { 0 } else { $image.Size })))
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]32)
    $writer.Write([uint32]$image.Bytes.Length)
    $writer.Write([uint32]$offset)
    $offset += $image.Bytes.Length
}
foreach ($image in $images) {
    $writer.Write($image.Bytes)
}
$writer.Dispose()
$file.Dispose()

Write-Output "Generated $OutputPath"
