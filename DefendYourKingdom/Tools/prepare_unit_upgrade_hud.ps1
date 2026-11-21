$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'hud_alpha_mask.ps1')
$city = Join-Path $PSScriptRoot '../SourceArt/CityHUD'
$folder = Join-Path $city 'UnitUpgrade'

function Patch($target, $source, $area, $sample, [int]$feather = 7, [switch]$MatchTone)
{
    $red=0.0; $green=0.0; $blue=0.0
    if ($MatchTone) {
        $count=0
        foreach ($i in 1..18) {
            $fraction=$i/19.0
            foreach ($point in @(@($fraction,0.0),@($fraction,1.0),@(0.0,$fraction),@(1.0,$fraction))) {
                $old=$target.GetPixel($area[0]+[int](($area[2]-1)*$point[0]),$area[1]+[int](($area[3]-1)*$point[1]))
                $new=$source.GetPixel($sample[0]+[int](($sample[2]-1)*$point[0]),$sample[1]+[int](($sample[3]-1)*$point[1]))
                $red+=$old.R-$new.R; $green+=$old.G-$new.G; $blue+=$old.B-$new.B
                ++$count
            }
        }
        $red/=$count; $green/=$count; $blue/=$count
    }
    for ($y=0; $y -lt $area[3]; ++$y) {
        for ($x=0; $x -lt $area[2]; ++$x) {
            $edge=[Math]::Min([Math]::Min($x,$area[2]-1-$x),[Math]::Min($y,$area[3]-1-$y))
            $blend=$feather -gt 0 ? [Math]::Min(1,$edge/[double]$feather) : 1
            $px=$area[0]+$x
            $py=$area[1]+$y
            $old=$target.GetPixel($px,$py)
            $new=$source.GetPixel($sample[0]+[int][Math]::Floor($x*$sample[2]/$area[2]),
                $sample[1]+[int][Math]::Floor($y*$sample[3]/$area[3]))
            $target.SetPixel($px,$py,[Drawing.Color]::FromArgb($old.A,
                [int][Math]::Clamp($old.R+($new.R+$red-$old.R)*$blend,0,255),
                [int][Math]::Clamp($old.G+($new.G+$green-$old.G)*$blend,0,255),
                [int][Math]::Clamp($old.B+($new.B+$blue-$old.B)*$blend,0,255)))
        }
    }
}

function Clear-Title($target, $source)
{
    # Use the cloth immediately surrounding each sample glyph to preserve its folds and color.
    $left=487; $top=101; $width=473; $height=90
    $mask=[bool[,]]::new($width,$height)
    for ($y=5; $y -lt $height-5; ++$y) {
        for ($x=5; $x -lt $width-5; ++$x) {
            $pixel=$source.GetPixel($left+$x,$top+$y)
            if ($pixel.G -lt 100 -or $pixel.G -lt $pixel.R*.57) { continue }
            for ($dy=-5; $dy -le 5; ++$dy) {
                for ($dx=-5; $dx -le 5; ++$dx) {
                    if ($dx*$dx+$dy*$dy -le 25) { $mask[($x+$dx),($y+$dy)]=$true }
                }
            }
        }
    }
    for ($y=0; $y -lt $height; ++$y) {
        for ($x=0; $x -lt $width; ++$x) {
            if (!$mask[$x,$y]) { continue }
            $red=0.0; $green=0.0; $blue=0.0; $weight=0.0
            foreach ($direction in @(@(-1,0),@(1,0),@(0,-1),@(0,1))) {
                $sx=$x+$direction[0]; $sy=$y+$direction[1]; $steps=1
                while ($sx -ge 0 -and $sx -lt $width -and $sy -ge 0 -and $sy -lt $height -and $mask[$sx,$sy]) {
                    $sx+=$direction[0]; $sy+=$direction[1]; ++$steps
                }
                if ($sx -lt 0 -or $sx -ge $width -or $sy -lt 0 -or $sy -ge $height) { continue }
                $pixel=$source.GetPixel($left+$sx,$top+$sy)
                $red+=$pixel.R/$steps; $green+=$pixel.G/$steps; $blue+=$pixel.B/$steps
                $weight+=1.0/$steps
            }
            if ($weight -gt 0) {
                $target.SetPixel($left+$x,$top+$y,[Drawing.Color]::FromArgb(
                    [int]($red/$weight),[int]($green/$weight),[int]($blue/$weight)))
            }
        }
    }
}

$reference=[Drawing.Bitmap]::new((Join-Path $folder 'Reference.png'))
$parchment=[Drawing.Bitmap]::new((Join-Path $city 'Modular/WindowBase.png'))
$base=$reference.Clone()
$parts=$reference.Clone()
try {
    # Keep the wood, red banner, decorative borders and row frames from the reference.
    Clear-Title $base $reference
    Patch $base $parchment @(671,280,554,165) @(295,435,554,165) 15 -MatchTone
    Patch $base $parchment @(225,205,401,312) @(254,303,401,312) 4

    $rowTops=@(518,611,705)
    foreach ($row in 0..2) {
        $top=$rowTops[$row]
        $valueLeft=$row -eq 2 ? 615 : 565
        # Replace the sample values with the neighboring brown strip; the labels stay engraved.
        Patch $base $reference @($valueLeft,($top+11),(745-$valueLeft),($row -eq 2 ? 88 : 59)) @(730,($top+11),20,($row -eq 2 ? 88 : 59))
        # The filled slot is drawn by the widget; the base starts with three empty slots.
        $slotY=$top+12
        $slotHeight=$row -eq 2 ? 76 : 65
        Patch $base $reference @(1047,$slotY,69,$slotHeight) @(1170,$slotY,69,$slotHeight) 2
        $captionY=$row -eq 2 ? 733 : $top+21
        $captionHeight=$row -eq 2 ? 47 : 42
        Patch $parts $reference @(842,$captionY,165,$captionHeight) @(834,$captionY,8,$captionHeight) 4
        $buttonY=@(529,622,722)[$row]
        $buttonHeight=@(62,62,70)[$row]
        $bottom=$buttonY+$buttonHeight-1
        $shape=New-HudPolygon @(762,($buttonY+16),778,$buttonY,1009,$buttonY,1025,($buttonY+16),
            1025,($bottom-15),1009,$bottom,778,$bottom,762,($bottom-15))
        Add-HudSprite $parts $parts $shape ([Drawing.Rectangle]::new(762,$buttonY,264,$buttonHeight))
        $shape.Dispose()
    }
    foreach ($plate in @(@(290,916,103,46),@(501,916,99,46),@(723,916,91,46),@(901,916,123,46),@(1151,916,91,46))) {
        Remove-HudInk $base $reference ([Drawing.Rectangle]::new($plate[0],$plate[1],$plate[2],$plate[3])) {param($p) $p.G -ge 123}
    }
    $slotShape=New-HudPolygon @(1055,543,1066,532,1101,532,1111,542,1111,574,1102,585,1065,585,1055,574)
    Add-HudSprite $parts $parts $slotShape ([Drawing.Rectangle]::new(1054,532,58,56))
    $slotShape.Dispose()
    $base.Save((Join-Path $folder 'UnitUpgradeBase.png'),[Drawing.Imaging.ImageFormat]::Png)
    $parts.Save((Join-Path $folder 'UnitUpgradeParts.png'),[Drawing.Imaging.ImageFormat]::Png)
} finally {
    $parts.Dispose()
    $base.Dispose()
    $parchment.Dispose()
    $reference.Dispose()
}
