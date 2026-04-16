param(
  [switch]$Build,
  [switch]$StartTeamSpeak,
  [switch]$StopTeamSpeak,
  [string]$BuildDir
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) {
  $msvcQt5Build = Join-Path $repoRoot 'build_msvc_qt5'
  $msvcBuild = Join-Path $repoRoot 'build_msvc_nmake'
  $mingwBuild = Join-Path $repoRoot 'build_mingw'
  if (Test-Path $msvcQt5Build) {
    $BuildDir = $msvcQt5Build
  } elseif (Test-Path $msvcBuild) {
    $BuildDir = $msvcBuild
  } else {
    $BuildDir = $mingwBuild
  }
}

$buildDir = $BuildDir
$installDir = Join-Path $buildDir 'install'
$installPlugins = Join-Path $installDir 'plugins'
$installSupport = Join-Path $installPlugins 'rp_soundboard_ultimate'
$tsRoot = 'C:\Program Files\TeamSpeak 3 Client'
$tsExe = Join-Path $tsRoot 'ts3client_win64.exe'
$tsPlugins = Join-Path $env:APPDATA 'TS3Client\plugins'
$tsPluginSupport = Join-Path $tsPlugins 'rp_soundboard_ultimate'
$tsLogs = Join-Path $env:APPDATA 'TS3Client\logs'
$loaderLog = Join-Path $env:APPDATA 'TS3Client\rpsu_loader.log'

function Get-TeamSpeakProcess {
  Get-Process -Name 'ts3client_win64' -ErrorAction SilentlyContinue
}

function Stop-TeamSpeakIfRequested {
  $process = Get-TeamSpeakProcess
  if (-not $process) {
    return
  }

  try {
    $process | Stop-Process -Force
    Start-Sleep -Seconds 2
  } catch {
    throw "TeamSpeak is running and could not be stopped from this shell. Close it manually or rerun this script from the same elevation level as TeamSpeak."
  }
}

if ($Build) {
  if ($buildDir -like '*build_msvc_*') {
    $msvcCommand = "call ""C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"" && ""C:\Program Files\CMake\bin\cmake.exe"" --build ""$buildDir"" && ""C:\Program Files\CMake\bin\cmake.exe"" --install ""$buildDir"" --prefix ""$installDir"""
    & cmd /c $msvcCommand
    if ($LASTEXITCODE -ne 0) {
      throw 'MSVC build or install staging failed.'
    }
  } else {
    & cmake --build $buildDir --config Release
    if ($LASTEXITCODE -ne 0) {
      throw 'Build failed.'
    }

    & cmake --install $buildDir --config Release --prefix $installDir
    if ($LASTEXITCODE -ne 0) {
      throw 'Install staging failed.'
    }
  }
}

if (-not (Test-Path $installPlugins)) {
  throw "Missing staged plugin directory: $installPlugins"
}
if (-not (Test-Path $installSupport)) {
  throw "Missing staged plugin support directory: $installSupport"
}

if ($StopTeamSpeak -or $StartTeamSpeak) {
  Stop-TeamSpeakIfRequested
} elseif (Get-TeamSpeakProcess) {
  throw 'TeamSpeak is running. Close it first, or rerun with -StopTeamSpeak.'
}

New-Item -ItemType Directory -Force -Path $tsPlugins | Out-Null
New-Item -ItemType Directory -Force -Path $tsPluginSupport | Out-Null
New-Item -ItemType Directory -Force -Path $tsLogs | Out-Null

$rootPluginFiles = @(
  'rp_soundboard_ultimate_win64.dll'
)

$legacyLooseFiles = @(
  'Qt5Core.dll',
  'Qt5Gui.dll',
  'Qt5Network.dll',
  'Qt5Widgets.dll',
  'libgcc_s_seh-1.dll',
  'libstdc++-6.dll',
  'libwinpthread-1.dll',
  'ffmpeg.exe',
  'yt-dlp.exe',
  'rp_soundboard_ultimate_runtime.bin',
  'rpsu_ui_preview.exe'
)

$pluginDirs = @(
  'platforms',
  'rp_soundboard_ultimate'
)

foreach ($name in $rootPluginFiles + $legacyLooseFiles) {
  $target = Join-Path $tsPlugins $name
  if (Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Force -ErrorAction SilentlyContinue
  }
}

foreach ($name in $pluginDirs) {
  $target = Join-Path $tsPlugins $name
  if (Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Recurse -Force -ErrorAction SilentlyContinue
  }
}

foreach ($name in $rootPluginFiles) {
  $source = Join-Path $installPlugins $name
  if (Test-Path -LiteralPath $source) {
    Copy-Item -LiteralPath $source -Destination (Join-Path $tsPlugins $name) -Force
  }
}

Get-ChildItem -LiteralPath $installSupport -Force | ForEach-Object {
  $destination = Join-Path $tsPluginSupport $_.Name
  Copy-Item -LiteralPath $_.FullName -Destination $destination -Recurse -Force
}

$null = robocopy $installSupport $tsPluginSupport /MIR /NFL /NDL /NJH /NJS /NP
$robocopyExit = $LASTEXITCODE
if ($robocopyExit -ge 8) {
  throw "robocopy support sync failed with exit code $robocopyExit"
}

$latestBefore = Get-ChildItem $tsLogs -Filter 'ts3client_*.log' -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
$loaderBefore = if (Test-Path $loaderLog) { Get-Item $loaderLog } else { $null }
$loaderBeforeLineCount = if ($loaderBefore) { (Get-Content $loaderLog -ErrorAction SilentlyContinue).Count } else { 0 }

$started = $null
if ($StartTeamSpeak) {
  if (-not (Test-Path $tsExe)) {
    throw "Missing TeamSpeak executable: $tsExe"
  }

  $started = Start-Process -FilePath $tsExe -PassThru
  Start-Sleep -Seconds 8
}

$latestAfter = Get-ChildItem $tsLogs -Filter 'ts3client_*.log' -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1

$logLines = @()
if ($latestAfter) {
  $logLines = Select-String -Path $latestAfter.FullName -Pattern 'rp_soundboard_ultimate|Initializing RP Soundboard Ultimate plugin|loaded in safe mode|runtime init failed|Failed to load plugin|LoadLibrary error|Required plugin function|Api version is not compatible' |
    ForEach-Object { $_.Line }
}

$loaderAfterLines = @()
if (Test-Path $loaderLog) {
  $loaderAfterLines = Get-Content $loaderLog -ErrorAction SilentlyContinue |
    Select-Object -Skip $loaderBeforeLineCount |
    ForEach-Object { $_.ToString() }
}

$runtimeInitialized = $false
$safeModeLoaded = $false
$runtimeLoadFailure = $false
foreach ($line in $loaderAfterLines) {
  if ($line -match 'ts3plugin_init: runtime initialized') {
    $runtimeInitialized = $true
  }
  if ($line -match 'loaded in safe mode|safe mode init') {
    $safeModeLoaded = $true
  }
  if ($line -match 'LoadLibraryExW failed|runtime init failed|missing one or more required exports') {
    $runtimeLoadFailure = $true
  }
}

[pscustomobject]@{
  deployed_from = $installPlugins
  deployed_to = $tsPlugins
  launched_process_id = if ($started) { $started.Id } else { $null }
  latest_log = if ($latestAfter) { $latestAfter.FullName } else { $null }
  latest_log_was_new = if ($latestBefore -and $latestAfter) { $latestAfter.FullName -ne $latestBefore.FullName } elseif ($latestAfter) { $true } else { $false }
  deployed_plugin_size = (Get-Item (Join-Path $tsPlugins 'rp_soundboard_ultimate_win64.dll')).Length
  runtime_initialized = $runtimeInitialized
  safe_mode_loaded = $safeModeLoaded
  runtime_load_failure = $runtimeLoadFailure
  plugin_log_lines = $logLines
  loader_log_lines = $loaderAfterLines
} | ConvertTo-Json -Depth 4
