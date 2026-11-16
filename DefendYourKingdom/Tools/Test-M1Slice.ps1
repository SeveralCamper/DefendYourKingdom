[CmdletBinding()]
param(
    [string]$EngineRoot = (Join-Path $env:ProgramFiles 'Epic Games/UE_5.5'),
    [switch]$SkipBuild,
    [switch]$Render
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$project = Join-Path $projectRoot 'DefendYourKingdom.uproject'
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$build = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
foreach ($required in @($project, $editor, $build)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Missing required file: $required" }
}
# Each invocation owns a new profile and reports. Never delete or reset play saves.
$runRoot = Join-Path $projectRoot ('Saved/M111/run-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
$evidence = Join-Path $runRoot 'evidence'
$userDir = Join-Path $runRoot 'user'
$shaderDir = Join-Path $projectRoot 'Saved/M111/Shaders'
$cacheDir = Join-Path $projectRoot 'DerivedDataCache'
New-Item -ItemType Directory -Force -Path $evidence, $userDir, $shaderDir, $cacheDir | Out-Null
$previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
$results = [System.Collections.Generic.List[object]]::new()
try {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $cacheDir, 'Process')
    if (-not $SkipBuild) {
        foreach ($target in @('DefendYourKingdomEditor', 'DefendYourKingdom')) {
            & $build $target Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE -NoUBA -NoUBALocal -DisableAdaptiveUnity "-Log=$runRoot/$target-build.log"
            if ($LASTEXITCODE -ne 0) { throw "$target build failed: $LASTEXITCODE" }
        }
    }
    foreach ($phase in @('play', 'reload', 'interrupt', 'recover')) {
        $report = Join-Path $runRoot $phase
        $log = Join-Path $runRoot "$phase.log"
        $arguments = @(
            ('"{0}"' -f $project), '-game', '-nosound', '-unattended', '-NoSplash',
            '-DDC=InstalledNoZenLocalFallback', ('-SessionId={0}' -f [Guid]::NewGuid().ToString('N')),
            ('-UserDir="{0}/"' -f $userDir.Replace('\', '/')),
            ('-ReportExportPath="{0}"' -f $report), ('-abslog="{0}"' -f $log),
            '-ExecCmds="Automation RunTests DYK.M111.RuntimeSliceAcrossProcesses"',
            "-DYKM111Phase=$phase", ('-DYKM111EvidenceDir="{0}"' -f $evidence)
        )
        if ($phase -ne 'interrupt') { $arguments += '-TestExit="Automation Test Queue Empty"' }
        if ($Render) {
            $arguments += @('-d3d11', '-sm5', '-windowed', '-ResX=1280', '-ResY=720', '-DYKM111Capture', ('-ShaderWorkingDir="{0}"' -f $shaderDir))
        } else { $arguments += '-NullRHI' }
        $process = Start-Process -FilePath $editor -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru
        $forced = $false
        try {
            $deadline = [DateTime]::UtcNow.AddSeconds(360)
            $indexPath = Join-Path $report 'index.json'
            while (-not $process.HasExited) {
                if ($phase -eq 'interrupt' -and (Test-Path -LiteralPath $indexPath)) {
                    # Wait for the completed Automation report as well as the active-wave
                    # marker, then kill only the process created above. No graceful travel.
                    $markerPath = Join-Path $evidence 'interrupt-complete.json'
                    $completedReport = $null
                    try { $completedReport = Get-Content -LiteralPath $indexPath -Raw | ConvertFrom-Json } catch {
                        # The reporter may still be flushing the JSON file.
                    }
                    if ($completedReport -and $completedReport.inProcess -eq 0 -and
                        $completedReport.failed -eq 0 -and $completedReport.notRun -eq 0 -and
                        ($completedReport.succeeded + $completedReport.succeededWithWarnings) -eq 1 -and
                        $completedReport.tests[0].fullTestPath -eq 'DYK.M111.RuntimeSliceAcrossProcesses' -and
                        (Test-Path -LiteralPath $markerPath)) {
                        $marker = Get-Content -LiteralPath $markerPath -Raw | ConvertFrom-Json
                        if ($marker.activeWave -ne $true -or $marker.paused -ne $true) { throw 'Interruption was not observed in a paused active wave.' }
                        Stop-Process -Id $process.Id -ErrorAction Stop
                        $forced = $true
                        $process.WaitForExit()
                        break
                    }
                }
                if ([DateTime]::UtcNow -gt $deadline) { throw "Timeout in $phase; inspect $log" }
                Start-Sleep -Milliseconds 250
                $process.Refresh()
            }
            $process.WaitForExit()
            if (-not $forced -and $process.ExitCode -ne 0) { throw "$phase exited with $($process.ExitCode); inspect $log" }
            if ($phase -eq 'interrupt' -and -not $forced) { throw 'Interruption process exited before the forced-close check.' }
            $index = Get-Content -LiteralPath $indexPath -Raw | ConvertFrom-Json
            if ($index.failed -ne 0 -or $index.notRun -ne 0 -or $index.inProcess -ne 0 -or
                ($index.succeeded + $index.succeededWithWarnings) -ne 1 -or
                $index.tests[0].fullTestPath -ne 'DYK.M111.RuntimeSliceAcrossProcesses') {
                throw "Automation phase $phase did not pass exactly one requested test; inspect $report"
            }
            if (-not (Test-Path -LiteralPath (Join-Path $evidence "$phase-complete.json"))) { throw "Missing $phase completion marker" }
            $results.Add([pscustomobject]@{ phase = $phase; pid = $process.Id; forcedClose = $forced; report = $indexPath; succeeded = $index.succeeded; warnings = $index.succeededWithWarnings })
            Write-Output "PASS $phase ($report)"
        } finally {
            if (-not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
        }
    }
    $results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $runRoot 'results.json') -Encoding utf8
    Write-Output "M1-SLICE cross-process checks passed: $runRoot"
} finally {
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
}
