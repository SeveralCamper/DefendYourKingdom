[CmdletBinding()]
param(
    [string]$EngineRoot = (Join-Path $env:ProgramFiles 'Epic Games/UE_5.5'),
    [switch]$Render
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$project = Join-Path $projectRoot 'DefendYourKingdom.uproject'
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$runRoot = Join-Path $projectRoot ('Saved/M203/runtime-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
$evidence = Join-Path $runRoot 'evidence'
$userDir = Join-Path $runRoot 'user'
$shaders = Join-Path $projectRoot 'Saved/M203/Shaders'
New-Item -ItemType Directory -Force -Path $evidence,$userDir,$shaders | Out-Null
$priorCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $projectRoot 'DerivedDataCache'),'Process')
    foreach ($phase in @('play','reload')) {
        $report = Join-Path $runRoot $phase
        $arguments = @(
            ('"{0}"' -f $project), '-game','-nosound','-unattended','-NoSplash','-DDC=InstalledNoZenLocalFallback',
            ('-UserDir="{0}/"' -f $userDir.Replace('\','/')),
            ('-ReportExportPath="{0}"' -f $report), ('-abslog="{0}/{1}.log"' -f $runRoot,$phase),
            '-ExecCmds="Automation RunTests DYK.M203.RuntimeEditorAcrossProcesses"',
            '-TestExit="Automation Test Queue Empty"', "-DYKM203Phase=$phase", ('-DYKM203Evidence="{0}"' -f $evidence)
        )
        if ($Render) {
            $arguments += @('-d3d11','-sm5','-windowed','-ResX=1280','-ResY=720','-DYKM203Capture',('-ShaderWorkingDir="{0}"' -f $shaders))
        } else { $arguments += '-NullRHI' }
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
        try {
            if (-not $process.WaitForExit(240000)) { throw "Timeout: $phase" }
            if ($process.ExitCode -ne 0) { throw "$phase process exit $($process.ExitCode)" }
            $result = Get-Content -LiteralPath (Join-Path $report 'index.json') -Raw | ConvertFrom-Json
            if ($result.failed -ne 0 -or $result.notRun -ne 0 -or $result.inProcess -ne 0 -or ($result.succeeded+$result.succeededWithWarnings) -ne 1) {
                throw "Automation failed: $report"
            }
            Write-Output "PASS $phase ($report)"
        } finally {
            if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        }
    }
    if ($Render) {
        foreach ($capture in @('01-buffer.png','02-committed.png')) {
            if (-not (Test-Path -LiteralPath (Join-Path $evidence $capture))) { throw "Missing capture: $capture" }
        }
    }
    Write-Output "M2.03 runtime evidence: $runRoot"
} finally {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$priorCache,'Process')
}
