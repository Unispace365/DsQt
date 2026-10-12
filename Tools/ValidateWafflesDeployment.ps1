<#
.SYNOPSIS
Read-only checks for a ProjectCloner/ECPresenter-style Windows Waffles deployment.
.DESCRIPTION
Checks the installed files against the application's CMake cache and source assets.
Does not launch the app, connect to Bridge, change settings, or certify device acceptance.
Run separately for Debug and Release. Qt libraries are compared by version, not hash,
because windeployqt patches QtCore. Missing optional settings hooks are warnings.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDirectory,
    [Parameter(Mandatory)][string]$DeployBin,
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')][string]$Configuration = 'Release',
    [ValidateSet('Web', 'Pdf', 'Keyboard', 'Touch')][string[]]$Features = @('Web', 'Pdf', 'Keyboard', 'Touch'),
    [string[]]$AssetGroups = @('quick_menu', 'content_launcher', 'window')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$failures = [Collections.Generic.List[string]]::new()
$notes = [Collections.Generic.List[string]]::new()
function Require-File([string]$Path) {
    if (!(Test-Path -LiteralPath $Path -PathType Leaf)) { $failures.Add("Missing: $Path"); return $false }
    return $true
}
function Cache-Value([string]$Key) {
    $match = [regex]::Match($cache, '(?m)^' + [regex]::Escape($Key) + ':[^=]+=(.*)$')
    if (!$match.Success) { throw "CMake cache has no $Key" }
    return $match.Groups[1].Value.Trim()
}
function Ini-Value([string]$Text, [string]$Key, [string]$Default) {
    $match = [regex]::Match($Text, '(?m)^\s*' + [regex]::Escape($Key) + '\s*=\s*(.*?)\s*$')
    if ($match.Success) { return $match.Groups[1].Value.Trim('"') }
    return $Default
}
function Check-X64([string]$Path) {
    if (!(Require-File $Path)) { return }
    $stream = [IO.File]::OpenRead($Path)
    $reader = [IO.BinaryReader]::new($stream)
    try {
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664) {
            $failures.Add("Not a Windows x64 PE binary: $Path")
        }
    } finally { $reader.Dispose() }
}

$BuildDirectory = (Resolve-Path -LiteralPath $BuildDirectory).Path
$DeployBin = (Resolve-Path -LiteralPath $DeployBin).Path
$cache = Get-Content -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Raw
$sourceDirectory = Cache-Value 'CMAKE_HOME_DIRECTORY'
$qtKit = [IO.Path]::GetFullPath((Join-Path (Cache-Value 'Qt6_DIR') '../../..'))
$suffix = if ($Configuration -eq 'Debug') { 'd' } else { '' }
$compiler = Cache-Value 'CMAKE_CXX_COMPILER'
if ($compiler -notmatch '(?i)(MSVC|MICROS~).*[/\\](x64|HOSTX64)[/\\]') {
    $notes.Add('Check the compiler architecture manually; this validator targets MSVC x64.')
}
$qtVersion = (Get-Item -LiteralPath (Join-Path $qtKit "bin/Qt6Core$suffix.dll")).VersionInfo.FileVersion
Write-Output "Kit from CMake cache: $qtKit ($qtVersion), $Configuration"
$qtConf = Join-Path $DeployBin 'qt.conf'
if (!(Require-File $qtConf)) { throw 'Cannot resolve deployed Qt paths without qt.conf.' }
$ini = Get-Content -LiteralPath $qtConf -Raw
$prefix = [IO.Path]::GetFullPath((Join-Path $DeployBin (Ini-Value $ini 'Prefix' '.')))
$plugins = Join-Path $prefix (Ini-Value $ini 'Plugins' 'plugins')
$qml = Join-Path $prefix (Ini-Value $ini 'QmlImports' 'qml')
$resources = Join-Path $prefix (Ini-Value $ini 'Data' '.')

foreach ($key in @('QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH')) {
    if ([Environment]::GetEnvironmentVariable($key)) { $notes.Add("$key is set; clear developer overrides for the installed smoke test.") }
}
$executables = @(Get-ChildItem -LiteralPath $DeployBin -Filter 'app*.exe')
if (!$executables.Count) { $failures.Add('No app*.exe found in deployment bin.') }
foreach ($exe in $executables) { Check-X64 $exe.FullName }

$modules = @('Core', 'Gui', 'Qml', 'Quick', 'QuickControls2', 'Multimedia', 'CanvasPainter', 'Canvas2D', 'Sql')
$imports = @('QtQuick', 'QtQuick/Controls', 'QtQuick/Layouts', 'QtQuick/VectorImage', 'QtMultimedia', 'QtCanvas2D', 'Dsqt/Core', 'Dsqt/Bridge', 'Dsqt/Waffles')
$pluginFiles = @("platforms/qwindows$suffix.dll", "imageformats/qsvg$suffix.dll", "multimedia/ffmpegmediaplugin$suffix.dll", "sqldrivers/qsqlite$suffix.dll")
if ('Web' -in $Features) {
    $modules += @('WebEngineCore', 'WebEngineQuick')
    $imports += 'QtWebEngine'
    Check-X64 (Join-Path $DeployBin "QtWebEngineProcess$suffix.exe")
    $resourceSuffix = if ($Configuration -eq 'Debug') { '.debug' } else { '' }
    foreach ($name in @('icudtl.dat', "qtwebengine_resources$resourceSuffix.pak", "qtwebengine_resources_100p$resourceSuffix.pak", "qtwebengine_resources_200p$resourceSuffix.pak", "v8_context_snapshot$resourceSuffix.bin")) {
        [void](Require-File (Join-Path $resources "resources/$name"))
    }
}
if ('Pdf' -in $Features) { $modules += @('Pdf', 'PdfQuick'); $imports += 'QtQuick/Pdf' }
if ('Keyboard' -in $Features) {
    $modules += 'VirtualKeyboard'
    $imports += @('QtQuick/VirtualKeyboard', 'QtQuick/VirtualKeyboard/Settings')
    $pluginFiles += "platforminputcontexts/qtvirtualkeyboardplugin$suffix.dll"
}
if ('Touch' -in $Features) { $imports += 'Dsqt/Touch' }
foreach ($module in $modules) { [void](Require-File (Join-Path $DeployBin "Qt6$module$suffix.dll")) }
foreach ($module in $imports) { [void](Require-File (Join-Path $qml "$module/qmldir")) }
foreach ($plugin in $pluginFiles) {
    $deployedPlugin = Join-Path $plugins $plugin
    Check-X64 $deployedPlugin
    $kitPlugin = Join-Path $qtKit "plugins/$plugin"
    if ((Test-Path -LiteralPath $deployedPlugin) -and (Test-Path -LiteralPath $kitPlugin)) {
        if ((Get-Item -LiteralPath $deployedPlugin).VersionInfo.FileVersion -ne (Get-Item -LiteralPath $kitPlugin).VersionInfo.FileVersion) {
            $failures.Add("Qt plugin version mismatch: $plugin")
        }
    }
}
Check-X64 (Join-Path $DeployBin 'tomlplusplus-3.dll')

# Compare every deployed Qt library to this kit, including optional transitive modules.
foreach ($dll in Get-ChildItem -LiteralPath $DeployBin -Filter 'Qt6*.dll') {
    Check-X64 $dll.FullName
    $kitDll = Join-Path $qtKit "bin/$($dll.Name)"
    if (!(Test-Path -LiteralPath $kitDll)) { $failures.Add("Qt library is absent from selected kit: $($dll.Name)"); continue }
    if ($dll.VersionInfo.FileVersion -ne (Get-Item -LiteralPath $kitDll).VersionInfo.FileVersion) {
        $failures.Add("Qt version mismatch: $($dll.Name)")
    }
    if ($Configuration -eq 'Debug' -and $dll.BaseName -cnotmatch 'd$') { $failures.Add("Release Qt DLL in Debug deployment: $($dll.Name)") }
    if ($Configuration -ne 'Debug' -and $dll.BaseName -cmatch 'd$' -and $dll.BaseName -ne 'Qt6VirtualKeyboard') {
        # Module names such as VirtualKeyboard end in d themselves; identify via the kit's pair.
        if (Test-Path -LiteralPath (Join-Path $qtKit ('bin/' + $dll.BaseName.TrimEnd('d') + '.dll'))) {
            $failures.Add("Debug Qt DLL in Release deployment: $($dll.Name)")
        }
    }
}

foreach ($pattern in @('avcodec-*.dll', 'avformat-*.dll', 'avutil-*.dll', 'swresample-*.dll', 'swscale-*.dll')) {
    $kitFiles = @(Get-ChildItem -LiteralPath (Join-Path $qtKit 'bin') -Filter $pattern)
    if (!$kitFiles.Count) { $failures.Add("No FFmpeg runtime in the selected kit: $pattern") }
    foreach ($dll in $kitFiles) { Check-X64 (Join-Path $DeployBin $dll.Name) }
}

# Runtime discovery intentionally excludes developer PATH. A Debug CRT is not redistributable.
foreach ($name in @("msvcp140$suffix.dll", "vcruntime140$suffix.dll", "vcruntime140_1$suffix.dll")) {
    if (!(Test-Path -LiteralPath (Join-Path $DeployBin $name)) -and !(Test-Path -LiteralPath (Join-Path $env:SystemRoot "System32/$name"))) {
        if ($Configuration -eq 'Debug') { $notes.Add("Debug launch requires the matching compiler runtime directory: $name") }
        else { $failures.Add("Missing MSVC runtime: $name (install the matching x64 redistributable)") }
    }
}

foreach ($group in $AssetGroups) {
    $sourceAssets = Join-Path $sourceDirectory "data/images/waffles/$group"
    if (!(Test-Path -LiteralPath $sourceAssets)) { $failures.Add("No source asset group: $sourceAssets"); continue }
    $icons = @(Get-ChildItem -LiteralPath $sourceAssets -Filter '*.svg')
    if (!$icons.Count) { $failures.Add("No SVG icons in $sourceAssets") }
    foreach ($icon in $icons) {
        $deployed = Join-Path $DeployBin "data/images/waffles/$group/$($icon.Name)"
        if (Require-File $deployed) {
            if ((Get-FileHash -LiteralPath $deployed).Hash -ne (Get-FileHash -LiteralPath $icon.FullName).Hash) {
                $failures.Add("Stale deployed icon: $deployed")
            }
        }
    }
    Write-Output "Checked $($icons.Count) $group SVG assets."
}
$settings = Join-Path $DeployBin 'settings'
foreach ($name in @('engine.toml', 'app_settings.toml', 'bridgesync.toml', 'waffles_content.toml')) {
    [void](Require-File (Join-Path $settings $name))
}
if (Test-Path -LiteralPath (Join-Path $settings 'bridgesyc.toml')) { $notes.Add('Stale bridgesyc.toml is present; create a clean deployment before release.') }
$engineFile = Join-Path $settings 'engine.toml'
if (Test-Path -LiteralPath $engineFile) {
    $engineText = Get-Content -LiteralPath $engineFile -Raw
    # Standard starter-file references; this does not parse arbitrary TOML or local override layers.
    foreach ($match in [regex]::Matches($engineText, '"([^"\r\n]+\.toml)"')) {
        $name = $match.Groups[1].Value
        if (!(Test-Path -LiteralPath (Join-Path $settings $name))) { $notes.Add("Settings hook has no deployed file: $name; confirm it is intentionally optional or supplied by a local override.") }
    }
    if ($engineText -notmatch '"bridgesync\.toml"' -or $engineText -notmatch '"waffles_content\.toml"') {
        $failures.Add('engine.toml must load bridgesync.toml and waffles_content.toml in its extra settings collections.')
    }
}
foreach ($note in $notes) { Write-Warning $note }
foreach ($failure in $failures) { Write-Output "FAIL: $failure" }
if ($failures.Count) { throw "$($failures.Count) deployment file checks failed." }
Write-Output 'Deployment file checks passed. Still verify configured Bridge/platform/cache, feature startup and physical touch on the target device.'
