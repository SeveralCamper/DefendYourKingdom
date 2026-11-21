Add-Type -AssemblyName System.Drawing

function New-HudPolygon([double[]]$Coordinates)
{
    $points = for ($i=0; $i -lt $Coordinates.Count; $i+=2) {
        [Drawing.PointF]::new($Coordinates[$i],$Coordinates[$i+1])
    }
    $path=[Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddPolygon([Drawing.PointF[]]$points)
    return $path
}

function Get-HudMaskedImage($Source, $Path)
{
    $result=[Drawing.Bitmap]::new($Source.Width,$Source.Height,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics=[Drawing.Graphics]::FromImage($result)
    $brush=[Drawing.TextureBrush]::new($Source)
    try {
        $graphics.SmoothingMode=[Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.FillPath($brush,$Path)
    } finally {
        $brush.Dispose()
        $graphics.Dispose()
    }
    return $result
}

function Add-HudSprite($Canvas, $Source, $Path, $Rect)
{
    $sample=$Source.Clone($Rect,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $transform=[Drawing.Drawing2D.Matrix]::new()
    $transform.Translate(-$Rect.X,-$Rect.Y)
    $Path.Transform($transform)
    $sprite=Get-HudMaskedImage $sample $Path
    $graphics=[Drawing.Graphics]::FromImage($Canvas)
    try {
        $graphics.CompositingMode=[Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.DrawImageUnscaled($sprite,$Rect.X,$Rect.Y)
    } finally {
        $graphics.Dispose()
        $sprite.Dispose()
        $sample.Dispose()
        $transform.Dispose()
    }
}

function Remove-HudInk($Target,$Source,[Drawing.Rectangle]$Region,[scriptblock]$IsInk)
{
    $mask=[bool[,]]::new($Region.Width,$Region.Height)
    $directions=@(@(-1,0),@(1,0),@(0,-1),@(0,1))
    for($y=4;$y -lt $Region.Height-4;++$y){
        for($x=4;$x -lt $Region.Width-4;++$x){
            $pixel=$Source.GetPixel($Region.X+$x,$Region.Y+$y)
            if(!(& $IsInk $pixel)){continue}
            for($dy=-4;$dy -le 4;++$dy){
                for($dx=-4;$dx -le 4;++$dx){
                    if($dx*$dx+$dy*$dy -le 16){$mask[($x+$dx),($y+$dy)]=$true}
                }
            }
        }
    }
    for($y=0;$y -lt $Region.Height;++$y){
        for($x=0;$x -lt $Region.Width;++$x){
            if(!$mask[$x,$y]){continue}
            $red=0.0; $green=0.0; $blue=0.0; $weight=0.0
            foreach($direction in $directions){
                $sx=$x+$direction[0]; $sy=$y+$direction[1]; $steps=1
                while($sx -ge 0 -and $sy -ge 0 -and $sx -lt $Region.Width -and $sy -lt $Region.Height -and $mask[$sx,$sy]){
                    $sx+=$direction[0]; $sy+=$direction[1]; ++$steps
                }
                if($sx -lt 0 -or $sy -lt 0 -or $sx -ge $Region.Width -or $sy -ge $Region.Height){continue}
                $pixel=$Source.GetPixel($Region.X+$sx,$Region.Y+$sy)
                $red+=$pixel.R/$steps; $green+=$pixel.G/$steps; $blue+=$pixel.B/$steps
                $weight+=1.0/$steps
            }
            if($weight -gt 0){
                $Target.SetPixel($Region.X+$x,$Region.Y+$y,[Drawing.Color]::FromArgb(
                    [int]($red/$weight),[int]($green/$weight),[int]($blue/$weight)))
            }
        }
    }
}
