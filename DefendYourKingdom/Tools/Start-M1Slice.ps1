[CmdletBinding()]
param(
    [string]$EngineRoot = (Join-Path $env:ProgramFiles 'Epic Games/UE_5.5')
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$project = Join-Path $projectRoot 'DefendYourKingdom.uproject'
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor.exe'
$module = Join-Path $projectRoot 'Binaries/Win64/UnrealEditor-DefendYourKingdom.dll'
foreach ($required in @($project, $editor, $module)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required file missing: $required. Build DefendYourKingdomEditor Win64 Development with UE5.5 first."
    }
}

# This persistent demonstration profile is separate from both normal play and QA.
$userDir = Join-Path $projectRoot 'Saved/M1Slice/User'
$shaderDir = Join-Path $projectRoot 'Saved/M1Slice/Shaders'
$cacheDir = Join-Path $projectRoot 'DerivedDataCache'
New-Item -ItemType Directory -Force -Path $userDir, $shaderDir, $cacheDir | Out-Null
$arguments = @(
    ('"{0}"' -f $project), '-game', '-nosound', '-NoSplash',
    '-d3d11', '-sm5', '-windowed', '-ResX=1280', '-ResY=720',
    '-DDC=InstalledNoZenLocalFallback',
    ('-UserDir="{0}/"' -f $userDir.Replace('\', '/')),
    ('-ShaderWorkingDir="{0}"' -f $shaderDir.Replace('\', '/')),
    ('-abslog="{0}"' -f (Join-Path $projectRoot 'Saved/M1Slice/game.log'))
)
# Intentionally interactive: the author plays in this window. No map override;
# GameDefaultMap must select the city on every fresh process.
$previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $cacheDir, 'Process')
    $gameProcess = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Normal -PassThru
} finally {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
}
Write-Output "M1-SLICE PID=$($gameProcess.Id); profile=$userDir"
