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

function Extend-UnitBoard($Source)
{
    $extra=132
    $board=[Drawing.Bitmap]::new($Source.Width,$Source.Height+$extra,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics=[Drawing.Graphics]::FromImage($board)
    $graphics.DrawImageUnscaled($Source,0,0)
    $graphics.Dispose()
    for($x=0;$x -lt $Source.Width;++$x){
        # Move the original lower corners intact. The seam across the parchment starts
        # below the resource plates; at the sides it runs above the corner hardware.
        $interior=[Math]::Clamp([Math]::Min(($x-188)/6.0,(1260-$x)/6.0),0,1)
        $center=[Math]::Clamp([Math]::Min(($x-211)/12.0,(1238-$x)/12.0),0,1)
        $seam=942+[int](44*$interior+13*$center)
        $depth=18-15*$interior
        for($y=$seam;$y -lt $board.Height;++$y){
            $offset=$y-$seam
            $sourceY=if($offset -ge $extra){$y-$extra}else{
                $seam-1-[int]($depth*[Math]::Min($offset,$extra-1-$offset)/(($extra-2)/2.0))
            }
            $board.SetPixel($x,$y,$Source.GetPixel($x,$sourceY))
        }
    }
    return $board
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
        # All three rows use the same button and cell sprites over their original plates.
        $controlsY=$top+5
        $controlsHeight=$row -eq 2 ? 98 : 76
        Patch $base $reference @(750,$controlsY,487,$controlsHeight) @(1030,$controlsY,15,$controlsHeight) 6
    }
    foreach ($plate in @(@(290,916,103,46),@(501,916,99,46),@(723,916,91,46),@(901,916,123,46),@(1151,916,91,46))) {
        Remove-HudInk $base $reference ([Drawing.Rectangle]::new($plate[0],$plate[1],$plate[2],$plate[3])) {param($p) $p.G -ge 123}
    }
    Patch $parts $reference @(842,539,165,42) @(834,539,8,42) 4
    $shape=New-HudPolygon @(762,545,778,529,1009,529,1025,545,1025,575,1009,590,778,590,762,575)
    Add-HudSprite $parts $parts $shape ([Drawing.Rectangle]::new(762,529,264,62))
    $shape.Dispose()
    foreach($slot in @(1054,1113)){
        $shape=New-HudPolygon @(($slot+1),543,($slot+12),532,($slot+47),532,($slot+57),542,($slot+57),574,
            ($slot+48),585,($slot+11),585,($slot+1),574)
        Add-HudSprite $parts $parts $shape ([Drawing.Rectangle]::new($slot,532,58,56))
        $shape.Dispose()
    }
    $popup=[Drawing.Bitmap]::new((Join-Path $city 'ProductionTemplate/Compact/ReferenceMilitary.png'))
    $cancel=$popup.Clone([Drawing.Rectangle]::new(613,797,422,88),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $shape=New-HudPolygon @(0,21,22,0,400,0,420,21,420,64,399,87,22,87,0,64)
    Add-HudSprite $parts $cancel $shape ([Drawing.Rectangle]::new(0,0,422,88))
    $shape.Dispose()
    $cancel.Dispose()
    $popup.Dispose()
    $extended=Extend-UnitBoard $base
    $extended.Save((Join-Path $folder 'UnitUpgradeBase.png'),[Drawing.Imaging.ImageFormat]::Png)
    $extended.Dispose()
    $parts.Save((Join-Path $folder 'UnitUpgradeParts.png'),[Drawing.Imaging.ImageFormat]::Png)
} finally {
    $parts.Dispose()
    $base.Dispose()
    $parchment.Dispose()
    $reference.Dispose()
}

function Save-StudySprite($Reference,$Name,[double[]]$Outline)
{
    $source=[Drawing.Bitmap]::new((Join-Path $folder $Reference))
    $shape=New-HudPolygon $Outline
    $sprite=Get-HudMaskedImage $source $shape
    try {
        $sprite.Save((Join-Path $folder "$Name.png"),[Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $sprite.Dispose()
        $shape.Dispose()
        $source.Dispose()
    }
}

Save-StudySprite 'LearnUnitReference.png' 'UnitStudyLearn' @(
    7,55, 22,42, 32,48, 38,37, 46,29, 55,22, 271,22, 284,18, 295,11,
    304,18, 317,22, 535,22, 545,28, 553,37, 560,49, 569,42, 583,55, 569,68, 560,62,
    554,73, 546,82, 535,90, 317,90, 305,94, 295,103, 285,94, 273,90, 55,90,
    46,85, 38,77, 31,65, 22,68)

$studyLearn=[Drawing.Bitmap]::new((Join-Path $folder 'UnitStudyLearn.png'))
$studyHover=$studyLearn.Clone()
try {
    $edgeSamples=@(@(-4,0),@(4,0),@(0,-4),@(0,4),@(-3,-3),@(3,-3),@(-3,3),@(3,3))
    for($y=4;$y -lt $studyLearn.Height-4;++$y){
        for($x=4;$x -lt $studyLearn.Width-4;++$x){
            $pixel=$studyLearn.GetPixel($x,$y)
            if($pixel.A -eq 0){continue}
            foreach($sample in $edgeSamples){
                if($studyLearn.GetPixel($x+$sample[0],$y+$sample[1]).A -ge 128){continue}
                $studyHover.SetPixel($x,$y,[Drawing.Color]::FromArgb($pixel.A,
                    [int]($pixel.R+(255-$pixel.R)*.38),[int]($pixel.G+(221-$pixel.G)*.38),
                    [int]($pixel.B+(118-$pixel.B)*.38)))
                break
            }
        }
    }
    $studyHover.Save((Join-Path $folder 'UnitStudyLearnHover.png'),[Drawing.Imaging.ImageFormat]::Png)
} finally {
    $studyHover.Dispose()
    $studyLearn.Dispose()
}

Save-StudySprite 'UnitStudiedReference.png' 'UnitStudyComplete' @(
    10,74, 59,43, 154,43, 172,46, 175,35, 184,26, 190,35, 199,24, 215,15, 214,27,
    242,26, 258,20, 276,18, 294,18, 310,22, 323,27, 750,27, 745,15, 762,21, 774,34,
    780,27, 788,36, 793,48, 809,43, 915,43, 963,74, 915,106, 794,106, 782,111,
    797,115, 785,121, 760,124, 728,121, 314,120, 298,125, 280,128, 262,126, 245,121,
    215,125, 194,122, 179,117, 193,111, 177,106, 58,106)
