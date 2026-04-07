$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$stagePlugins = Join-Path $repoRoot 'build_mingw\stage\plugins'
$tsRoot = 'C:\Program Files\TeamSpeak 3 Client'
$tsExe = Join-Path $tsRoot 'ts3client_win64.exe'
$tsPlugins = Join-Path $env:APPDATA 'TS3Client\plugins'
$tsPluginSupport = Join-Path $tsPlugins 'rp_soundboard_ultimate'
$tsLogs = Join-Path $env:APPDATA 'TS3Client\logs'
$pluginPrefix = 'rp_soundboard_ultimate'

if (-not (Test-Path $stagePlugins)) {
  throw "Missing staged plugin directory: $stagePlugins"
}

if (-not (Test-Path $tsExe)) {
  throw "Missing TeamSpeak executable: $tsExe"
}

New-Item -ItemType Directory -Force -Path $tsPlugins | Out-Null
New-Item -ItemType Directory -Force -Path $tsLogs | Out-Null
New-Item -ItemType Directory -Force -Path $tsPluginSupport | Out-Null

$existing = Get-Process | Where-Object { $_.ProcessName -eq 'ts3client_win64' }
if ($existing) {
  try {
    $existing | Stop-Process -Force
  } catch {
    throw "TeamSpeak is already running and could not be stopped from this shell. Close TeamSpeak manually, then rerun scripts\test-teamspeak.ps1."
  }
  Start-Sleep -Seconds 2
}

$latestBefore = Get-ChildItem $tsLogs -Filter 'ts3client_*.log' -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

$rootFiles = @(
  'rp_soundboard_ultimate_win64.dll'
)

$supportFiles = @(
  'yt-dlp.exe',
  'rpsu_ui_preview.exe',
  'rp_soundboard_ultimate_runtime.bin',
  'Qt5Core.dll',
  'Qt5Gui.dll',
  'Qt5Network.dll',
  'Qt5Widgets.dll',
  'libgcc_s_seh-1.dll',
  'libstdc++-6.dll',
  'libwinpthread-1.dll'
)

$legacyRootFiles = @(
  'yt-dlp.exe',
  'rp_soundboard_ultimate_runtime.bin',
  'rpsu_ui_preview.exe',
  'Qt5Core.dll',
  'Qt5Gui.dll',
  'Qt5Network.dll',
  'Qt5Widgets.dll',
  'libgcc_s_seh-1.dll',
  'libstdc++-6.dll',
  'libwinpthread-1.dll'
)

foreach ($name in $legacyRootFiles) {
  $legacyPath = Join-Path $tsPlugins $name
  if (Test-Path $legacyPath) {
    Remove-Item -LiteralPath $legacyPath -Force
  }
}

$existingDashboard = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -eq 'rpsu_ui_preview' }
if ($existingDashboard) {
  $existingDashboard | Stop-Process -Force -ErrorAction SilentlyContinue
}

$legacyPlatformsDir = Join-Path $tsPlugins 'platforms'
if (Test-Path $legacyPlatformsDir) {
  Remove-Item -LiteralPath $legacyPlatformsDir -Recurse -Force
}

foreach ($name in $rootFiles) {
  $source = Join-Path $stagePlugins $name
  if (-not (Test-Path $source)) {
    throw "Missing staged file: $source"
  }
  Copy-Item -LiteralPath $source -Destination (Join-Path $tsPlugins $name) -Force
}

$supportSourceDir = Join-Path $stagePlugins 'rp_soundboard_ultimate'
foreach ($name in $supportFiles) {
  $source = Join-Path $supportSourceDir $name
  if (-not (Test-Path $source)) {
    throw "Missing staged file: $source"
  }
  Copy-Item -LiteralPath $source -Destination (Join-Path $tsPluginSupport $name) -Force
}

$platformSource = Join-Path $supportSourceDir 'platforms\qwindows.dll'
$platformDestDir = Join-Path $tsPluginSupport 'platforms'
New-Item -ItemType Directory -Force -Path $platformDestDir | Out-Null
Copy-Item -LiteralPath $platformSource -Destination (Join-Path $platformDestDir 'qwindows.dll') -Force

$started = Start-Process -FilePath $tsExe -PassThru
Start-Sleep -Seconds 8

$pluginDll = (Join-Path $tsPlugins 'rp_soundboard_ultimate_win64.dll').Replace('\', '\\')
$invokeSource = @"
using System;
using System.Runtime.InteropServices;

public static class RpsuPluginInvoke {
  [DllImport("$pluginDll", CallingConvention = CallingConvention.Cdecl)]
  public static extern void ts3plugin_configure(IntPtr handle, IntPtr parent);
}
"@

Add-Type -TypeDefinition $invokeSource -Language CSharp
[RpsuPluginInvoke]::ts3plugin_configure([IntPtr]::Zero, [IntPtr]::Zero)
Start-Sleep -Seconds 3

$dashboardProcess = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.ProcessName -eq 'rpsu_ui_preview' } | Select-Object -First 1

$latestAfter = Get-ChildItem $tsLogs -Filter 'ts3client_*.log' |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

if (-not $latestAfter) {
  throw 'No TeamSpeak log found after launch.'
}

$logText = Get-Content -LiteralPath $latestAfter.FullName -Raw
$pluginLines = Select-String -InputObject $logText -Pattern 'rp_soundboard_ultimate|LoadLibrary error|Failed to load plugin|qstrcmp|qHash|Einsprungpunkt|entry point' |
  ForEach-Object { $_.Line }

$result = [pscustomobject]@{
  launched_process_id = $started.Id
  log_file = $latestAfter.FullName
  latest_log_was_new = if ($latestBefore) { $latestAfter.FullName -ne $latestBefore.FullName } else { $true }
  deployed_loader_timestamp = (Get-Item (Join-Path $tsPlugins 'rp_soundboard_ultimate_win64.dll')).LastWriteTime
  deployed_runtime_timestamp = (Get-Item (Join-Path $tsPluginSupport 'rp_soundboard_ultimate_runtime.bin')).LastWriteTime
  dashboard_started = [bool]$dashboardProcess
  dashboard_process_id = if ($dashboardProcess) { $dashboardProcess.Id } else { $null }
  plugin_log_lines = $pluginLines
}

$result | ConvertTo-Json -Depth 4
