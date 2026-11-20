$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$production = Join-Path $PSScriptRoot '../SourceArt/CityHUD/ProductionTemplate'
$folder = Join-Path $production 'Compact'
$cropTop = 96

function Copy-FeatheredPatch($target, $source, [Drawing.Rectangle]$area, [Drawing.Rectangle]$sample)
{
    for ($y = 0; $y -lt $area.Height; $y++) {
        for ($x = 0; $x -lt $area.Width; $x++) {
            $edge = [Math]::Min([Math]::Min($x, $area.Width-1-$x), [Math]::Min($y, $area.Height-1-$y))
            $mix = [Math]::Min(1, $edge / 7.0)
            $old = $target.GetPixel($area.X+$x, $area.Y+$y)
            $new = $source.GetPixel(
                $sample.X + [int][Math]::Floor($x * $sample.Width / $area.Width),
                $sample.Y + [int][Math]::Floor($y * $sample.Height / $area.Height))
            $target.SetPixel($area.X+$x, $area.Y+$y, [Drawing.Color]::FromArgb(
                $old.A, [int]($old.R+($new.R-$old.R)*$mix),
                [int]($old.G+($new.G-$old.G)*$mix), [int]($old.B+($new.B-$old.B)*$mix)))
        }
    }
}

$ribbon = [Drawing.Bitmap]::new((Join-Path $production 'FirstBase.png'))
$blankStats = [Drawing.Bitmap]::new((Join-Path $folder 'ReferenceMilitary.png'))
try {
    foreach ($kind in @('Production', 'Military')) {
        $source = [Drawing.Bitmap]::new((Join-Path $folder "Reference$kind.png"))
        try {
            # Preserve the complete frame and all authored button lettering.
            Copy-FeatheredPatch $source $ribbon ([Drawing.Rectangle]::new(400,247,407,80)) ([Drawing.Rectangle]::new(369,155,352,62))
            Copy-FeatheredPatch $source $blankStats ([Drawing.Rectangle]::new(812,421,200,47)) ([Drawing.Rectangle]::new(355,421,200,47))
            if ($kind -eq 'Production') {
                Copy-FeatheredPatch $source $blankStats ([Drawing.Rectangle]::new(363,422,96,48)) ([Drawing.Rectangle]::new(363,422,96,48))
                Copy-FeatheredPatch $source $blankStats ([Drawing.Rectangle]::new(666,422,88,48)) ([Drawing.Rectangle]::new(666,422,88,48))
            }
            $base = $source.Clone([Drawing.Rectangle]::new(0,$cropTop,$source.Width,1056),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
            try {
                $base.Save((Join-Path $folder "$($kind)Base.png"),[Drawing.Imaging.ImageFormat]::Png)
            } finally {
                $base.Dispose()
            }
            foreach ($button in @(
                @{Name='Collect'; X=227; Y=508; Width=753; Height=134; CutX=40; CutY=33},
                @{Name='Upgrade'; X=174; Y=797; Width=422; Height=88; CutX=23; CutY=22},
                @{Name='CancelWork'; X=613; Y=797; Width=422; Height=88; CutX=23; CutY=22}
            )) {
                $rect = [Drawing.Rectangle]::new($button.X,$button.Y,$button.Width,$button.Height)
                $gray = $source.Clone($rect,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
                try {
                    for ($y = 0; $y -lt $rect.Height; $y++) {
                        for ($x = 0; $x -lt $rect.Width; $x++) {
                            $side = [Math]::Min($x,$rect.Width-1-$x)
                            $vertical = [Math]::Min($y,$rect.Height-1-$y)
                            $contour = $side/$button.CutX + $vertical/$button.CutY - 1
                            $mix = [Math]::Clamp([Math]::Min($side,$vertical)/2.0,0,1)
                            $mix *= [Math]::Clamp($contour*12,0,1)
                            if ($mix -le 0) { continue }
                            $p = $gray.GetPixel($x,$y)
                            $luma = 0.21*$p.R + 0.72*$p.G + 0.07*$p.B
                            $gray.SetPixel($x,$y,[Drawing.Color]::FromArgb($p.A,
                                [int]($p.R+($luma-$p.R)*$mix),[int]($p.G+($luma-$p.G)*$mix),[int]($p.B+($luma-$p.B)*$mix)))
                        }
                    }
                    $gray.Save((Join-Path $folder "$kind$($button.Name)Disabled.png"),[Drawing.Imaging.ImageFormat]::Png)
                } finally {
                    $gray.Dispose()
                }
            }
        } finally {
            $source.Dispose()
        }
    }
} finally {
    $blankStats.Dispose()
    $ribbon.Dispose()
}
