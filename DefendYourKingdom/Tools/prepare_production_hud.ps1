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
 $b.Save((Join-Path $folder 'InteriorBase.png'),[Drawing.Imaging.ImageFormat]::Png)
 $a.Dispose();$b.Dispose()
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
  # Remove the transparent export margin so the button fills its original HUD slot.
  $crop=$order.Clone([Drawing.Rectangle]::new(194,124,1786,486),[Drawing.Imaging.PixelFormat]::Format32bppArgb)
  try {$crop.Save((Join-Path $folder 'OrderDisabled.png'),[Drawing.Imaging.ImageFormat]::Png)}
  finally {$crop.Dispose()}
 } finally {$order.Dispose()}
 Get-ChildItem $folder -Filter '*.png' | ForEach-Object{[pscustomobject]@{file=$_.Name;sha256=(Get-FileHash $_.FullName).Hash}} | ConvertTo-Json | Set-Content (Join-Path $folder 'manifest.json') -Encoding utf8
} finally {$first.Dispose();$inside.Dispose()}
