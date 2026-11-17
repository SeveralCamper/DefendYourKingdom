[CmdletBinding()]
param(
    [string]$Filter = 'DYK',
    [string]$EvidenceName = 'LocalAutomation',
    [string]$EngineRoot = (Join-Path $env:ProgramFiles 'Epic Games/UE_5.5')
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$runRoot = Join-Path $projectRoot ('Saved/' + $EvidenceName + '/automation-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Force $runRoot | Out-Null
$previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath','Process')
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',(Join-Path $projectRoot 'DerivedDataCache'),'Process')
    $arguments = @(
        ('"{0}"' -f (Join-Path $projectRoot 'DefendYourKingdom.uproject')),
        '-NullRHI','-nosound','-unattended','-NoSplash','-DDC=InstalledNoZenLocalFallback',
        ('-UserDir="{0}/user/"' -f $runRoot.Replace('\','/')),
        ('-abslog="{0}/automation.log"' -f $runRoot),
        ('-ReportExportPath="{0}/report"' -f $runRoot),
        ('-ExecCmds="Automation RunTests {0}"' -f $Filter),
        '-TestExit="Automation Test Queue Empty"'
    )
    $process = Start-Process -FilePath (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
    try {
        $deadline = [DateTime]::UtcNow.AddMinutes(10)
        while (-not $process.WaitForExit(1000)) {
            if ([DateTime]::UtcNow -gt $deadline) { throw "Automation timeout: $runRoot" }
        }
        if ($process.ExitCode -ne 0) { throw "Automation exit $($process.ExitCode): $runRoot" }
        $report = Get-Content -LiteralPath (Join-Path $runRoot 'report/index.json') -Raw | ConvertFrom-Json
        if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or ($report.succeeded + $report.succeededWithWarnings) -eq 0) { throw "Automation did not pass: $runRoot" }
        [pscustomobject]@{succeeded=$report.succeeded; warnings=$report.succeededWithWarnings; failed=$report.failed; report=$runRoot} | ConvertTo-Json
    } finally {
        if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    }
} finally { [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$previousCache,'Process') }
