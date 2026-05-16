param(
  [switch]$Build,
  [switch]$StartTeamSpeak,
  [switch]$StopTeamSpeak,
  [string]$BuildDir
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot

function Add-PathEntryIfExists {
  param([string]$PathEntry)
  if ((Test-Path -LiteralPath $PathEntry) -and (($env:PATH -split ';') -notcontains $PathEntry)) {
    $env:PATH = "$PathEntry;$env:PATH"
  }
}

$localMsysRoot = Join-Path $env:LOCALAPPDATA 'Programs\msys64'
Add-PathEntryIfExists (Join-Path $localMsysRoot 'ucrt64\bin')
Add-PathEntryIfExists (Join-Path $localMsysRoot 'usr\bin')

function Find-VcVars64 {
  $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
  if (Test-Path -LiteralPath $vswhere) {
    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($installPath) {
      $vcvars = Join-Path $installPath 'VC\Auxiliary\Build\vcvars64.bat'
      if (Test-Path -LiteralPath $vcvars) {
        return $vcvars
      }
    }
  }

  foreach ($edition in @('BuildTools', 'Community', 'Professional', 'Enterprise')) {
    $vcvars = Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\$edition\VC\Auxiliary\Build\vcvars64.bat"
    if (Test-Path -LiteralPath $vcvars) {
      return $vcvars
    }
  }

  throw 'Visual Studio 2022 C++ Build Tools were not found.'
}

function Find-CMakeExe {
  $command = Get-Command cmake -ErrorAction SilentlyContinue
  if ($command) {
    return $command.Source
  }

  $programFilesCMake = Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe'
  if (Test-Path -LiteralPath $programFilesCMake) {
    return $programFilesCMake
  }

  throw 'cmake.exe was not found.'
}

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
    & taskkill.exe /F /T /IM ts3client_win64.exe | Out-Null
    Start-Sleep -Seconds 2
  }

  if (Get-TeamSpeakProcess) {
    throw "TeamSpeak is running and could not be stopped from this shell. Close it manually or rerun this script from the same elevation level as TeamSpeak."
  }
}

if ($Build) {
  if ($buildDir -like '*build_msvc_*') {
    $vcvars = Find-VcVars64
    $cmakeExe = Find-CMakeExe
    $msvcCommand = "call ""$vcvars"" && ""$cmakeExe"" --build ""$buildDir"" && ""$cmakeExe"" --install ""$buildDir"" --prefix ""$installDir"""
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

$result = [pscustomobject]@{
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
}

$result | ConvertTo-Json -Depth 4

if ($StartTeamSpeak -and (-not $runtimeInitialized -or $safeModeLoaded -or $runtimeLoadFailure)) {
  throw 'TeamSpeak smoke test failed: RP Soundboard Ultimate did not initialize outside safe mode.'
}
