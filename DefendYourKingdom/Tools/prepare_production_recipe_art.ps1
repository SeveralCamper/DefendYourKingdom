param(
    [string]$ReferenceDirectory = (Join-Path $PSScriptRoot '../../concept-arts/HUD')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$output = Join-Path $PSScriptRoot '../SourceArt/CityHUD/ProductionTemplate/RecipeArt'
New-Item -ItemType Directory -Force -Path $output | Out-Null

# The artwork sits between the bevels and above the engraved nameplate.
$references = @(
    @{ Name = 'Bakery';  Left = @(243, 505, 761, 1018); Top = 268; Width = 211; Height = 163 }
    @{ Name = 'Sawmill'; Left = @(243, 507, 760, 1014); Top = 269; Width = 214; Height = 161 }
    @{ Name = 'Quarry';  Left = @(243, 505, 760, 1017); Top = 268; Width = 211; Height = 168 }
    @{ Name = 'Forge';   Left = @(243, 506, 762, 1018); Top = 274; Width = 210; Height = 165 }
)

foreach ($reference in $references) {
    $path = Join-Path $ReferenceDirectory ($reference.Name + 'EnterenceHUD.png')
    $source = [Drawing.Bitmap]::new($path)
    try {
        for ($index = 0; $index -lt $reference.Left.Count; $index++) {
            $rectangle = [Drawing.Rectangle]::new(
                $reference.Left[$index], $reference.Top, $reference.Width, $reference.Height)
            $artwork = $source.Clone($rectangle, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                for ($y = 0; $y -lt $artwork.Height; $y++) {
                    for ($x = 0; $x -lt $artwork.Width; $x++) {
                        # Blend the source ground into the template near the frame.
                        $distance = [Math]::Min(
                            [Math]::Min($x, $artwork.Width - 1 - $x),
                            [Math]::Min($y, $artwork.Height - 1 - $y))
                        if ($distance -ge 9) { continue }
                        $pixel = $artwork.GetPixel($x, $y)
                        $alpha = [int]($pixel.A * $distance / 9)
                        $artwork.SetPixel($x, $y, [Drawing.Color]::FromArgb($alpha, $pixel))
                    }
                }
                $name = '{0}_{1:00}.png' -f $reference.Name, ($index + 1)
                $artwork.Save((Join-Path $output $name), [Drawing.Imaging.ImageFormat]::Png)
            } finally {
                $artwork.Dispose()
            }
        }
    } finally {
        $source.Dispose()
    }
}
