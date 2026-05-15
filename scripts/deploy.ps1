# Build Release + deploy to live TS3 plugins folder.
# Closes anything holding the plugin files open first.
#
# Usage:
#   powershell -File scripts\deploy.ps1            # build + deploy + restart TS3
#   powershell -File scripts\deploy.ps1 -SkipBuild # deploy current build only
#   powershell -File scripts\deploy.ps1 -NoRestart # do not relaunch TS3

param(
  [switch]$SkipBuild,
  [switch]$NoRestart
)

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$src  = Join-Path $repo "build\Release"
$dst  = Join-Path $env:APPDATA "TS3Client\plugins"

$ts3Exe = "C:\Program Files\TeamSpeak 3 Client\ts3client_win64.exe"

function Stop-LockingProcesses {
  $names = @("ts3client_win64", "rpsu_ui_preview")
  $killed = @()
  foreach ($name in $names) {
    $procs = Get-Process -Name $name -ErrorAction SilentlyContinue
    foreach ($p in $procs) {
      try {
        $p.Kill()
        $p.WaitForExit(5000) | Out-Null
        $killed += "$name (PID $($p.Id))"
      } catch {
        Write-Warning "Could not kill $name (PID $($p.Id)): $_"
      }
    }
  }
  if ($killed.Count -gt 0) {
    Write-Host "Killed:" ($killed -join ", ")
    Start-Sleep -Milliseconds 500
  }
}

# Build
if (-not $SkipBuild) {
  Write-Host "Building Release..." -ForegroundColor Cyan
  cmake --build (Join-Path $repo "build") --config Release
  if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }
}

# Verify build outputs
$files = @(
  @{ Src = "rp_soundboard_ultimate_win64.dll";   Dst = "rp_soundboard_ultimate_win64.dll" }
  @{ Src = "rp_soundboard_ultimate_runtime.bin"; Dst = "rp_soundboard_ultimate\rp_soundboard_ultimate_runtime.bin" }
  @{ Src = "rpsu_ui_preview.exe";                Dst = "rp_soundboard_ultimate\rpsu_ui_preview.exe" }
)

foreach ($f in $files) {
  $p = Join-Path $src $f.Src
  if (-not (Test-Path $p)) { throw "Missing build output: $p" }
}

# Stop locking processes, then copy
$ts3WasRunning = $null -ne (Get-Process -Name "ts3client_win64" -ErrorAction SilentlyContinue)

Stop-LockingProcesses

Write-Host "Deploying to $dst" -ForegroundColor Cyan
foreach ($f in $files) {
  $srcPath = Join-Path $src $f.Src
  $dstPath = Join-Path $dst $f.Dst
  Copy-Item $srcPath $dstPath -Force
  Write-Host "  [ok] $($f.Dst)"
}

# Restart TS3 if it was running
if ($ts3WasRunning -and -not $NoRestart) {
  if (Test-Path $ts3Exe) {
    Write-Host "Restarting TS3..." -ForegroundColor Cyan
    Start-Process -FilePath $ts3Exe
  } else {
    Write-Warning "TS3 was running but not found at $ts3Exe -- start it manually."
  }
}

Write-Host "Done." -ForegroundColor Green
