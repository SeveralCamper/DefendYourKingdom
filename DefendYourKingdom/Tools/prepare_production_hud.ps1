$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root 'SourceArt/CityHUD/ProductionTemplate'
function Clear-Value($dst,$src,$x,$y,$w,$h,$sampleX){
 for($py=$y;$py -lt $y+$h;$py++){for($px=$x;$px -lt $x+$w;$px++){$dst.SetPixel($px,$py,$src.GetPixel($sampleX,$py))}}
}
$first=[Drawing.Bitmap]::new((Join-Path $folder 'ReferenceFirst.png'))
$inside=[Drawing.Bitmap]::new((Join-Path $folder 'ReferenceInterior.png'))
try {
 $a=$first.Clone([Drawing.Rectangle]::new(0,0,1122,1402),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
 $b=$inside.Clone([Drawing.Rectangle]::new(0,0,1448,1086),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
 # Keep each window intact. Only example values are cleared; buttons are never cut out.
 Clear-Value $a $first 342 690 90 55 435
 Clear-Value $a $first 630 690 90 55 719
 Clear-Value $a $first 759 693 188 40 750
 for($i=0;$i -lt 4;$i++){
  Clear-Value $b $inside (355+260*$i) 521 77 34 (295+260*$i)
  Clear-Value $b $inside (260+260*$i) 588 174 36 (265+260*$i)
 }
 Clear-Value $b $inside 463 819 92 50 450
 Clear-Value $b $inside 940 822 80 50 1024
 $a.Save((Join-Path $folder 'FirstBase.png'),[Drawing.Imaging.ImageFormat]::Png)
 $a.Dispose()
 $disabled=[Drawing.Bitmap]::new((Join-Path $folder 'ReferenceDisabled.png'))
 try {
  # Identical source coordinates: preserve the authored button and its shadow.
  # Fade only the surrounding parchment at the edge of each crop.
  $buttons=@(
   @('CollectDisabled',210,770,705,141),
   @('UpgradeDisabled',157,1043,404,107),
   @('CancelWorkDisabled',562,1043,403,107)
  )
  foreach($button in $buttons){
   $name,$x,$y,$w,$h=$button
   $crop=$disabled.Clone([Drawing.Rectangle]::new($x,$y,$w,$h),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
   try {
    for($py=0;$py -lt $h;$py++){for($px=0;$px -lt $w;$px++){
     $edge=[Math]::Min([Math]::Min($px,$w-1-$px),[Math]::Min($py,$h-1-$py))
     if($edge -ge 6){continue}
     $pixel=$crop.GetPixel($px,$py)
     $crop.SetPixel($px,$py,[Drawing.Color]::FromArgb([int]($pixel.A*$edge/6),$pixel.R,$pixel.G,$pixel.B))
    }}
    $crop.Save((Join-Path $folder ($name+'.png')),[Drawing.Imaging.ImageFormat]::Png)
   } finally {$crop.Dispose()}
  }
 } finally {$disabled.Dispose()}
 $order=[Drawing.Bitmap]::new((Join-Path $folder 'ReferenceOrderDisabled.png'))
 try {
  # Both states use the same pixel silhouette, traced from the original blue button.
  $w=224; $h=69
  $normal=$inside.Clone([Drawing.Rectangle]::new(236,646,$w,$h),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $gray=[Drawing.Bitmap]::new($w,$h,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $graphics=[Drawing.Graphics]::FromImage($gray)
  try {
   $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
   $graphics.DrawImage($order,[Drawing.Rectangle]::new(0,0,$w,$h),
    [Drawing.Rectangle]::new(194,124,1786,486),[Drawing.GraphicsUnit]::Pixel)
  } finally {$graphics.Dispose()}
  try {
   for($y=0;$y -lt $h;$y++){for($x=0;$x -lt $w;$x++){
    $horizontal=[Math]::Min($x,$w-1-$x)
    $vertical=[Math]::Min($y,$h-1-$y)
    # One common bevel, including antialias coverage. Both outputs have identical alpha.
    $alpha=[int](255*[Math]::Clamp(($horizontal+$vertical-15.0)/1.4,0,1))
    $bluePixel=$normal.GetPixel($x,$y)
    $grayPixel=$gray.GetPixel($x,$y)
    if($grayPixel.A -lt 250 -and $alpha -gt 0){
     $grayPixel=$gray.GetPixel([Math]::Clamp($x+4*[Math]::Sign(112-$x),0,223),[Math]::Clamp($y+4*[Math]::Sign(34-$y),0,68))
    }
    $normal.SetPixel($x,$y,[Drawing.Color]::FromArgb($alpha,$bluePixel.R,$bluePixel.G,$bluePixel.B))
    $gray.SetPixel($x,$y,[Drawing.Color]::FromArgb($alpha,$grayPixel.R,$grayPixel.G,$grayPixel.B))
   }}
   $normal.Save((Join-Path $folder 'OrderNormal.png'),[Drawing.Imaging.ImageFormat]::Png)
   $gray.Save((Join-Path $folder 'OrderDisabled.png'),[Drawing.Imaging.ImageFormat]::Png)
  } finally {$normal.Dispose();$gray.Dispose()}
 } finally {$order.Dispose()}
 # Rebuild the small parchment strip behind each baked button. The two separate
 # states now occupy the same shape, so old outlines cannot show through.
 foreach($position in @(236,498,757,1016)){
  for($y=646;$y -lt 721;$y++){
   for($x=$position-4;$x -le $position+226;$x++){
    $a0=$inside.GetPixel($x,644)
    $a1=$inside.GetPixel($x,721)
    $mix=($y-644)/77.0
    $b.SetPixel($x,$y,[Drawing.Color]::FromArgb(
     [int]($a0.R+($a1.R-$a0.R)*$mix),[int]($a0.G+($a1.G-$a0.G)*$mix),[int]($a0.B+($a1.B-$a0.B)*$mix)))
   }
  }
 }
 $b.Save((Join-Path $folder 'InteriorBase.png'),[Drawing.Imaging.ImageFormat]::Png)
 $b.Dispose()
 Get-ChildItem $folder -Filter '*.png' | ForEach-Object{[pscustomobject]@{file=$_.Name;sha256=(Get-FileHash $_.FullName).Hash}} | ConvertTo-Json | Set-Content (Join-Path $folder 'manifest.json') -Encoding utf8
} finally {$first.Dispose();$inside.Dispose()}
