$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'hud_alpha_mask.ps1')
$city=Join-Path $PSScriptRoot '../SourceArt/CityHUD'
$folder=Join-Path $city 'Recruitment'
$ref=[Drawing.Bitmap]::new((Join-Path $folder 'Reference.png'))
$base=$ref.Clone()
$paper=[Drawing.Bitmap]::new((Join-Path $city 'Modular/WindowBase.png'))
$atlas=[Drawing.Bitmap]::new(1448,1086,[Drawing.Imaging.PixelFormat]::Format32bppArgb)

function Sprite($x,$y,$w,$h,[double[]]$Outline)
{
    $path=New-HudPolygon $Outline
    try { Add-HudSprite $atlas $ref $path ([Drawing.Rectangle]::new($x,$y,$w,$h)) }
    finally { $path.Dispose() }
}

try {
    # Continuous blank paper from the existing authoring base under the six dynamic slots.
    # Preserve the reference's carved edges, corner flourishes and bottom divider.
    $surface=[Drawing.Bitmap]::new(1049,756)
    $graphics=[Drawing.Graphics]::FromImage($surface)
    $graphics.DrawImage($paper,[Drawing.Rectangle]::new(0,0,1049,756),220,226,1016,658,[Drawing.GraphicsUnit]::Pixel)
    $graphics.Dispose()
    for($y=0;$y -lt $surface.Height;++$y){
        for($x=0;$x -lt $surface.Width;++$x){
            $edge=[Math]::Min([Math]::Min($x,1048-$x),[Math]::Min($y,755-$y))
            $a=[Math]::Min(1,$edge/14.0)
            $old=$ref.GetPixel(202+$x,215+$y)
            $new=$surface.GetPixel($x,$y)
            $base.SetPixel(202+$x,215+$y,[Drawing.Color]::FromArgb(
                [int]($old.R+($new.R-$old.R)*$a),[int]($old.G+($new.G-$old.G)*$a),[int]($old.B+($new.B-$old.B)*$a)))
        }
    }
    $surface.Dispose()
    Remove-HudInk $base $ref ([Drawing.Rectangle]::new(554,102,350,78)) {param($p) $p.G -gt 105 -and $p.G -gt $p.R*.57}
    # The divider and its diamonds are drawn over the continuous parchment.
    foreach($line in @(@(221,920,310,17),@(916,920,313,17))){
        for($y=$line[1];$y -lt $line[1]+$line[3];++$y){
            for($x=$line[0];$x -lt $line[0]+$line[2];++$x){
                $pixel=$ref.GetPixel($x,$y)
                $alpha=[Math]::Clamp((151-$pixel.G)/64.0,0,1)
                if($alpha -eq 0){continue}
                $paperPixel=$base.GetPixel($x,$y)
                $base.SetPixel($x,$y,[Drawing.Color]::FromArgb(
                    [int]($paperPixel.R+($pixel.R-$paperPixel.R)*$alpha),
                    [int]($paperPixel.G+($pixel.G-$paperPixel.G)*$alpha),
                    [int]($paperPixel.B+($pixel.B-$paperPixel.B)*$alpha)))
            }
        }
    }

    $outline=New-HudPolygon @(
        49,266,66,249,87,232,93,218,82,211,70,199,66,182,72,168,88,159,105,155,105,135,115,123,130,120,
        139,93,147,81,174,69,205,63,215,65,220,83,259,84,284,80,311,78,339,80,365,73,
        392,61,412,55,447,44,478,39,517,37,549,42,565,45,588,39,585,18,607,24,616,13,628,39,643,35,658,25,
        659,19,686,10,723,1,753,8,787,18,789,33,804,38,817,14,828,37,859,17,849,41,871,47,891,41,
        922,38,956,39,1003,47,1040,58,1073,69,1100,78,1142,79,1155,84,1194,84,1245,84,1270,88,1290,82,
        1309,84,1326,94,1340,110,1347,128,1345,148,1336,171,1320,183,1320,240,1308,272,1298,301,1297,716,
        1308,752,1308,877,1317,885,1322,940,1335,945,1338,962,1337,992,1317,1015,1306,1035,1295,1042,
        1226,1042,1218,1036,225,1036,218,1042,147,1042,134,1020,111,997,112,946,125,939,134,860,
        140,848,143,597,125,638,109,575,92,590,89,584,96,409,87,406,77,403,74,389,79,380,95,374,
        143,357,142,334,131,319,128,323,109,341,101,352,93,353,86,343,77,337,63,331,58,307,54,278,48,271)
    $masked=Get-HudMaskedImage $base $outline
    $masked.Save((Join-Path $folder 'RecruitmentBase.png'))
    $masked.Dispose()
    $outline.Dispose()

    # Lock and rim share a source. The rim has a transparent hole for the live portrait.
    $card=@(578,574,870,574,887,591,887,858,870,875,578,875,560,858,560,591)
    Sprite 560 574 328 302 $card
    $rim=New-HudPolygon $card
    $rim.AddPolygon([Drawing.PointF[]]@(
        [Drawing.PointF]::new(582,583),[Drawing.PointF]::new(866,583),[Drawing.PointF]::new(878,595),
        [Drawing.PointF]::new(878,854),[Drawing.PointF]::new(866,866),[Drawing.PointF]::new(582,866),
        [Drawing.PointF]::new(569,854),[Drawing.PointF]::new(569,595)))
    Add-HudSprite $atlas $ref $rim ([Drawing.Rectangle]::new(560,574,328,302))
    $rimSprite=$atlas.Clone([Drawing.Rectangle]::new(560,574,328,302),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics=[Drawing.Graphics]::FromImage($atlas)
    $graphics.DrawImageUnscaled($rimSprite,218,574)
    $graphics.Dispose()
    Sprite 560 574 328 302 $card
    $rim.Dispose()
    $rimSprite.Dispose()

    $clean=$ref.Clone()
    Remove-HudInk $clean $ref ([Drawing.Rectangle]::new(285,492,207,40)) {param($p) $p.G -gt 119}
    Remove-HudInk $clean $ref ([Drawing.Rectangle]::new(591,243,40,48)) {param($p) $p.R -gt 118 -and $p.G -gt 110}
    foreach($piece in @(
        @{Bounds=@(230,482,310,59);Shape=@(231,499,244,484,518,484,536,499,536,519,516,540,246,540,231,525)},
        @{Bounds=@(569,227,79,87);Shape=@(570,244,609,229,646,242,642,279,633,298,609,313,584,299,574,281)}
    )){
        $path=New-HudPolygon $piece.Shape
        $b=$piece.Bounds
        Add-HudSprite $atlas $clean $path ([Drawing.Rectangle]::new($b[0],$b[1],$b[2],$b[3]))
        $path.Dispose()
    }
    $clean.Dispose()
    Sprite 214 236 152 72 @(215,260,228,247,253,243,360,238,363,242,350,262,359,283,349,287,249,294,222,305,216,300,219,280)
    Sprite 548 894 79 70 @(550,926,573,899,617,896,623,903,616,927,624,950,620,960,572,963)
    Sprite 823 894 79 70 @(824,903,833,897,875,897,901,927,877,958,831,962,825,953,834,928)
    foreach($circle in @(@(655,908,38,39),@(706,910,38,37))){
        $path=[Drawing.Drawing2D.GraphicsPath]::new()
        $path.AddEllipse([single]$circle[0],[single]$circle[1],[single]$circle[2],[single]$circle[3])
        Add-HudSprite $atlas $ref $path ([Drawing.Rectangle]::new($circle[0],$circle[1],$circle[2],$circle[3]))
        $path.Dispose()
    }
    $atlas.Save((Join-Path $folder 'RecruitmentParts.png'))
} finally {
    $atlas.Dispose()
    $paper.Dispose()
    $base.Dispose()
    $ref.Dispose()
}
