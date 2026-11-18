param([string]$Port, [ValidateSet('check','tuning')][string]$View='tuning', [string]$ViewerExe=$env:MCUVIEWER_EXE)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
if([string]::IsNullOrWhiteSpace($Port)) {
    Write-Output 'Usage: .\Start_MCUViewer.ps1 -Port COMxx (host wireless DAP serial port)'
    exit 1
}
if([string]::IsNullOrWhiteSpace($ViewerExe)) {
    $ViewerExe='D:\software2\MCUViewer_1.4.0\MCUViewer_Windows_1.4.0\MCUViewer-1.4.0-win64-portable\bin\MCUViewer.exe'
}
if(-not(Test-Path -LiteralPath $ViewerExe -PathType Leaf)) {throw 'Set MCUVIEWER_EXE or pass -ViewerExe with your MCUViewer executable path.'}
$viewerDirectory=Split-Path -Parent $ViewerExe
New-Item -ItemType Directory -Path (Join-Path $projectRoot 'build') -Force | Out-Null
$pythonPath=(Get-Command python -ErrorAction Stop).Source
& $pythonPath (Join-Path $projectRoot 'tools\prepare_mcuviewer.py') --port $Port
if($LASTEXITCODE -ne 0) {throw 'Profile generation failed'}
$bridgeScript=Join-Path $projectRoot 'tools\viewer_gdb_bridge.py'
$listener=Get-NetTCPConnection -LocalAddress 127.0.0.1 -LocalPort 3334 -State Listen -ErrorAction SilentlyContinue
if($listener){
    $bridgeProcess=Get-CimInstance Win32_Process -Filter ('ProcessId='+$listener.OwningProcess)
    if($bridgeProcess.CommandLine -notlike ('*'+$bridgeScript+'*') -or $bridgeProcess.CommandLine -notmatch ('--port\s+'+[regex]::Escape($Port)+'(?:\s|$)')) {throw 'Port3334 belongs to another process/COM port; close that bridge before continuing.'}
} else {
    Start-Process -FilePath $pythonPath -ArgumentList @(('"'+$bridgeScript+'"'),'--port',$Port) -WindowStyle Hidden -RedirectStandardOutput (Join-Path $projectRoot 'build\viewer_bridge.stdout.log') -RedirectStandardError (Join-Path $projectRoot 'build\viewer_bridge.log')
    for($attempt=0;$attempt -lt 20;$attempt++) {
        Start-Sleep -Milliseconds 250
        if(Get-NetTCPConnection -LocalAddress 127.0.0.1 -LocalPort 3334 -State Listen -ErrorAction SilentlyContinue){break}
    }
    if(-not(Get-NetTCPConnection -LocalAddress 127.0.0.1 -LocalPort 3334 -State Listen -ErrorAction SilentlyContinue)){throw 'Bridge failed to start; check build/viewer_bridge.log. COM port must be free.'}
}
$project=Join-Path $PSScriptRoot ($View+'.mcuvproj')
Start-Process -FilePath $ViewerExe -ArgumentList ('"'+$project+'"') -WorkingDirectory $viewerDirectory -WindowStyle Normal
Write-Output 'Select GDBSERVER,127.0.0.1:3334. Bridge uses the wireless DAP COM port at115200. Other modes are in mcuviewer/profiles. No OpenOCD server is needed.'
