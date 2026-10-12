param([string]$Keil = 'E:\keil5\UV4\UV4.exe')
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskProject = Join-Path $taskRoot 'MDK-ARM\STM32F405RGT6.uvprojx'
$taskLog = Join-Path $taskRoot 'MDK-ARM\freertos_build_final.log'
$taskStart = Get-Date
$taskArguments = '-r "' + $taskProject + '" -o "' + $taskLog + '"'
$taskProcess = Start-Process -FilePath $Keil -ArgumentList $taskArguments -WindowStyle Hidden -PassThru
if (-not $taskProcess.WaitForExit(60000)) {
    Stop-Process -Id $taskProcess.Id
    throw 'Keil did not exit within 60 seconds; check project XML and compiler configuration.'
}
if (-not (Test-Path -LiteralPath $taskLog)) { throw 'Keil did not create a build log.' }
$taskFile = Get-Item -LiteralPath $taskLog
if ($taskFile.LastWriteTime -lt $taskStart) { throw 'Keil did not update the build log.' }
$taskText = Get-Content -LiteralPath $taskLog -Raw
Write-Output $taskText
if ($taskProcess.ExitCode -ne 0 -or $taskText -notmatch '0 Error\(s\), 0 Warning\(s\)') {
    throw ('Keil build did not pass cleanly; exit code ' + $taskProcess.ExitCode)
}
