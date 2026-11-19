# Literal extraction of the author's references. No AI, resampling, or replacement artwork.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$taskRoot=Split-Path $PSScriptRoot -Parent
$taskSource=Join-Path $taskRoot 'SourceArt/CityHUD/Bakery'
$taskParts=Join-Path $taskSource 'Parts'
New-Item -ItemType Directory -Force $taskParts | Out-Null
$taskFirst=[Drawing.Bitmap]::new((Join-Path $taskSource 'ReferenceFirst.png'))
$taskInput=[Drawing.Bitmap]::new((Join-Path $taskSource 'ReferenceInterior.png'))
$taskInterior=$taskInput.Clone([Drawing.Rectangle]::new(0,0,$taskInput.Width,$taskInput.Height),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
$taskInput.Dispose()
$taskRows=[Collections.Generic.List[object]]::new()
function Save-Part($name,$source,$rect,$inside,$action='None',$index=0) {
    $box=[Drawing.Rectangle]::new($rect[0],$rect[1],$rect[2],$rect[3])
    $part=$source.Clone($box,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try { $part.Save((Join-Path $taskParts "$name.png"),[Drawing.Imaging.ImageFormat]::Png) } finally { $part.Dispose() }
    $taskRows.Add([pscustomobject]@{name=$name;rect=$rect;inside=$inside;action=$action;index=$index})
}
function Clear-Rect($image,$rect) {
    $g=[Drawing.Graphics]::FromImage($image)
    try { $g.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy; $brush=[Drawing.SolidBrush]::new([Drawing.Color]::Transparent); $g.FillRectangle($brush,$rect[0],$rect[1],$rect[2],$rect[3]); $brush.Dispose() } finally {$g.Dispose()}
}
function Blank-Number($image,$rect,$sampleX) {
    # Copy adjacent empty leather on the same scanline; no stretched patches or new colors.
    for($y=$rect[1];$y -lt $rect[1]+$rect[3];$y++) {
        for($x=$rect[0];$x -lt $rect[0]+$rect[2];$x++) { $image.SetPixel($x,$y,$image.GetPixel($sampleX+($x-$rect[0])%5,$y)) }
    }
}
try {
    # Only the outer black matte of reference 2 is removed. Interior pixels are untouched.
    for($y=0;$y -lt $taskInterior.Height;$y++) {
        $left=0
        while($left -lt $taskInterior.Width) { $c=$taskInterior.GetPixel($left,$y); if([Math]::Max($c.R,[Math]::Max($c.G,$c.B)) -gt 28){break}; $taskInterior.SetPixel($left,$y,[Drawing.Color]::Transparent); $left++ }
        $right=$taskInterior.Width-1
        while($right -gt $left) { $c=$taskInterior.GetPixel($right,$y); if([Math]::Max($c.R,[Math]::Max($c.G,$c.B)) -gt 28){break}; $taskInterior.SetPixel($right,$y,[Drawing.Color]::Transparent); $right-- }
    }
    # Remove connected exterior pockets between the lantern, leaves and wood as well.
    $w=$taskInterior.Width; $h=$taskInterior.Height
    $seen=[bool[]]::new($w*$h); $queue=[Collections.Generic.Queue[int]]::new()
    for($x=0;$x -lt $w;$x++){ $queue.Enqueue($x); $queue.Enqueue(($h-1)*$w+$x) }
    for($y=0;$y -lt $h;$y++){ $queue.Enqueue($y*$w); $queue.Enqueue($y*$w+$w-1) }
    while($queue.Count -gt 0) {
        $at=$queue.Dequeue(); if($seen[$at]){continue}; $seen[$at]=$true
        $x=$at%$w; $y=[int][Math]::Floor($at/$w); $c=$taskInterior.GetPixel($x,$y)
        if($c.A -gt 0 -and [Math]::Max($c.R,[Math]::Max($c.G,$c.B)) -gt 28){continue}
        $taskInterior.SetPixel($x,$y,[Drawing.Color]::Transparent)
        if($x -gt 0){$queue.Enqueue($at-1)}; if($x -lt $w-1){$queue.Enqueue($at+1)}
        if($y -gt 0){$queue.Enqueue($at-$w)}; if($y -lt $h-1){$queue.Enqueue($at+$w)}
    }
    $firstBase=$taskFirst.Clone([Drawing.Rectangle]::new(0,0,1122,1402),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $insideBase=$taskInterior.Clone([Drawing.Rectangle]::new(0,0,1448,1086),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $firstSpecs=@(
        @('Header',@(0,0,966,275),'None'),
        @('BuildingPicture',@(220,275,680,280),'None'),
        @('Description',@(174,555,780,100),'None'),
        @('Statistics',@(154,669,822,101),'None'),
        @('Collect',@(284,780,558,111),'Collect'),
        @('Upgrade',@(187,894,366,84),'Upgrade'),
        @('CancelWork',@(575,894,366,84),'CancelWork'),
        @('Visit',@(205,988,722,127),'Visit'),
        @('Rotate',@(188,1123,366,89),'Rotate'),
        @('Move',@(574,1123,367,89),'Move'),
        @('CloseFirst',@(966,84,143,145),'Close')
    )
    foreach($spec in $firstSpecs) { Save-Part $spec[0] $taskFirst $spec[1] $false $spec[2]; Clear-Rect $firstBase $spec[1] }
    $stats=[Drawing.Bitmap]::new((Join-Path $taskParts 'Statistics.png'))
    Blank-Number $stats @(213,25,75,43) 286
    Blank-Number $stats @(490,25,72,43) 563
    Blank-Number $stats @(646,25,159,43) 586
    Blank-Number $stats @(20,20,73,62) 286
    $stats.Save((Join-Path $taskParts 'StatisticsLive.png'),[Drawing.Imaging.ImageFormat]::Png); $stats.Dispose()
    # The common collect plate and the resource icon are independent assets.
    $collect=[Drawing.Bitmap]::new((Join-Path $taskParts 'Collect.png'))
    $icon=$collect.Clone([Drawing.Rectangle]::new(109,15,108,78),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for($y=0;$y -lt $icon.Height;$y++) { for($x=0;$x -lt $icon.Width;$x++) {
        $c=$icon.GetPixel($x,$y); if($c.R -lt 70 -or $c.R -lt $c.G*1.08){$icon.SetPixel($x,$y,[Drawing.Color]::Transparent)}
    }}
    $icon.Save((Join-Path $taskParts 'BreadIcon.png'),[Drawing.Imaging.ImageFormat]::Png); $icon.Dispose()
    Blank-Number $collect @(109,15,108,78) 65
    $collect.Save((Join-Path $taskParts 'CollectPlate.png'),[Drawing.Imaging.ImageFormat]::Png); $collect.Dispose()
    for($i=0;$i -lt 4;$i++) {
        $x=@(215,487,741,995)[$i]; $w=@(268,248,248,251)[$i]
        Save-Part "Offer$i" $taskInterior @($x,246,$w,494) $true
        Clear-Rect $insideBase @($x,246,$w,494)
        $bx=@(242,496,750,1004)[$i]
        Save-Part "Order$i" $taskInterior @($bx,638,225,77) $true 'Order' $i
        $card=[Drawing.Bitmap]::new((Join-Path $taskParts "Offer$i.png"))
        Clear-Rect $card @(($bx-$x),392,225,77)
        $card.Save((Join-Path $taskParts "OfferLive$i.png"),[Drawing.Imaging.ImageFormat]::Png); $card.Dispose()
    }
    # First recipe is not selected by default. Reuse an unselected author's frame.
    $neutral=[Drawing.Bitmap]::new((Join-Path $taskParts 'OfferLive1.png'))
    $g=[Drawing.Graphics]::FromImage($neutral)
    $g.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
    $g.DrawImage($taskInterior,[Drawing.Rectangle]::new(11,20,222,165),240,266,222,165,[Drawing.GraphicsUnit]::Pixel)
    $g.DrawImage($taskInterior,[Drawing.Rectangle]::new(18,259,210,121),247,505,210,121,[Drawing.GraphicsUnit]::Pixel)
    $g.Dispose()
    $neutral.Save((Join-Path $taskParts 'OfferLive0.png'),[Drawing.Imaging.ImageFormat]::Png); $neutral.Dispose()
    # Remove the baked outer glow including the padding above the old selected frame.
    for($y=232;$y -lt 747;$y++){for($x=210;$x -lt 483;$x++){$insideBase.SetPixel($x,$y,$taskInterior.GetPixel(484,$y))}}
    ($taskRows | Where-Object name -eq 'Offer0').rect=@(229,246,248,494)
    Save-Part 'CloseInterior' $taskInterior @(1245,78,104,105) $true 'Close'
    Save-Part 'BackInterior' $taskInterior @(1254,185,93,91) $true 'Back'
    Clear-Rect $insideBase @(1245,78,104,105); Clear-Rect $insideBase @(1254,185,93,91)
    Blank-Number $insideBase @(970,798,80,48) 769
    $firstBase.Save((Join-Path $taskParts 'FirstBase.png'),[Drawing.Imaging.ImageFormat]::Png)
    $insideBase.Save((Join-Path $taskParts 'InteriorBase.png'),[Drawing.Imaging.ImageFormat]::Png)
    $firstBase.Dispose(); $insideBase.Dispose()
    $taskRows | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $taskSource 'slices.json') -Encoding utf8
    Get-ChildItem $taskParts -Filter '*.png' | ForEach-Object {[pscustomobject]@{name=$_.Name;sha256=(Get-FileHash $_.FullName).Hash}} | ConvertTo-Json | Set-Content (Join-Path $taskSource 'hashes.json') -Encoding utf8
} finally { $taskFirst.Dispose(); $taskInterior.Dispose() }
