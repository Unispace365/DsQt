<#
.SYNOPSIS
Build a local DsQt source package for ProjectCloner applications.
.DESCRIPTION
Uses the Library manifest's dependency baseline and a snapshot of the working
tree. Produces Debug and Release together, using the caller's binary cache.
Does not update the vcpkg checkout, register a package, or install into Documents.
Run directly from an x64 MSVC developer shell, or through build_and_install.bat.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Qt,
    [string] $VcpkgRoot = $env:VCPKG_ROOT,
    [string] $PackageRoot,
    [string] $Modules = 'Core,Bridge,Waffles,Qsys,Spout,Touch,TouchEngine'
)
$ErrorActionPreference = 'Stop'
# Load the utility functions (including Get-FileHash) before entering the
# staging script; Windows PowerShell's nested-script autoload can miss them.
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1')
if ($Qt -match '^\d+\.\d+\.\d+$') { $Qt = "C:\Qt\$Qt\msvc2022_64" }
$kit = (Resolve-Path -LiteralPath $Qt).Path.TrimEnd('\', '/')
$arch = Split-Path -Leaf $kit
$versionDirectory = Split-Path -Parent $kit
$qtVersion = Split-Path -Leaf $versionDirectory
$qtRoot = Split-Path -Parent $versionDirectory
if ($arch -ne 'msvc2022_64' -or $qtVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw 'Select a Qt kit at <Qt root>/<version>/msvc2022_64.'
}
if ([version]$qtVersion -lt [version]'6.12.0') { throw 'This DsQt Core requires Qt 6.12 or newer.' }
if (!(Get-Command cl.exe -ErrorAction SilentlyContinue) -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'Select the x64 MSVC developer environment matching the Qt kit.'
}
if (!$VcpkgRoot) { throw 'Set VCPKG_ROOT or pass -VcpkgRoot.' }
$VcpkgRoot = (Resolve-Path -LiteralPath $VcpkgRoot).Path
$vcpkg = Join-Path $VcpkgRoot 'vcpkg.exe'
if (!(Test-Path -LiteralPath $vcpkg)) { throw "Missing vcpkg: $vcpkg" }
$library = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $library 'build')).TrimEnd('\', '/')
if (!$PackageRoot) { $PackageRoot = Join-Path $buildRoot "vcpkg-package/qt$qtVersion" }
$work = [IO.Path]::GetFullPath($PackageRoot).TrimEnd('\', '/')
# Staging resets only source/ and overlay/dsqt/. Keep the whole operation in
# a dedicated build directory, never in the source tree or vcpkg installation.
if (!$work.StartsWith($buildRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $VcpkgRoot.StartsWith($work + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $work.StartsWith($VcpkgRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $work -eq $VcpkgRoot) {
    throw 'PackageRoot must be a dedicated directory below Library/build, separate from VCPKG_ROOT.'
}
$moduleMetadata = Get-Content -LiteralPath (Join-Path $library 'dsqt-modules.json') -Raw | ConvertFrom-Json
$selectedModules = @('Core') + @($Modules.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ })
foreach ($module in $selectedModules) {
    if (!$moduleMetadata.modules.PSObject.Properties[$module]) { throw "Unknown DsQt module: $module" }
}
for ($pass = 0; $pass -lt 7; $pass++) {
    foreach ($module in @($selectedModules)) { $selectedModules += @($moduleMetadata.modules.$module.requires) }
    $selectedModules = @($selectedModules | Sort-Object -Unique)
}
$features = @($selectedModules | ForEach-Object { $moduleMetadata.modules.$_.feature } | Where-Object { $_ })
$metadataPath = Join-Path $work 'dsqt-local-package.json' 
if (Test-Path -LiteralPath $metadataPath) { Remove-Item -LiteralPath $metadataPath }

& (Join-Path $PSScriptRoot 'Test-DsqtPort.ps1') -QtVersion $qtVersion -QtRoot $qtRoot -QtArch $arch `
    -VcpkgRoot $VcpkgRoot -WorkRoot $work -StageOnly

$triplet = 'x64-windows-qt' + $qtVersion.Replace('.', '-')
$overlay = Join-Path $work 'overlay'
$triplets = Join-Path $work 'triplets'
$portManifestPath = Join-Path $overlay 'dsqt/vcpkg.json'
$portManifest = Get-Content -LiteralPath $portManifestPath -Raw | ConvertFrom-Json
$libraryManifest = Get-Content -LiteralPath (Join-Path $library 'vcpkg.json') -Raw | ConvertFrom-Json
$baseline = $libraryManifest.'builtin-baseline'

# A different local snapshot must also change the package version, so vcpkg's
# installed-state comparison cannot silently keep an older DsQt in an app.
# The staged port includes the source digest; hash the recipe and triplet too.
$inputs = @(Get-ChildItem -LiteralPath (Join-Path $overlay 'dsqt') -File | Sort-Object Name |
    ForEach-Object { "$($_.Name) $((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)" })
$inputs += (Get-FileHash -LiteralPath (Join-Path $triplets "$triplet.cmake") -Algorithm SHA256).Hash
$inputs += $baseline
$sha256 = [Security.Cryptography.SHA256]::Create()
try { $digest = [BitConverter]::ToString($sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes(($inputs -join "`n")))).Replace('-', '').ToLowerInvariant() }
finally { $sha256.Dispose() }
$baseVersion = $portManifest.version
if (!$baseVersion -or $portManifest.'version-semver' -or $portManifest.'version-string' -or $portManifest.'version-date') {
    throw 'The DsQt port must use the version field for preview and four-part release versions.'
}
$separator = if ($baseVersion.Contains('-')) { '.' } else { '-' }
$packageVersion = "$baseVersion${separator}local.h$($digest.Substring(0, 16))"
$portManifest.version = $packageVersion
$utf8 = New-Object Text.UTF8Encoding($false)
function Write-Json([string] $Path, $Value) {
    [IO.File]::WriteAllText($Path, (($Value | ConvertTo-Json -Depth 20).Replace("`r`n", "`n") + "`n"), $utf8)
}
Write-Json $portManifestPath $portManifest
$manifestRoot = Join-Path $work 'manifest'
New-Item -ItemType Directory -Force -Path $manifestRoot | Out-Null
Write-Json (Join-Path $manifestRoot 'vcpkg.json') ([ordered]@{
    name = 'dsqt-local-build'
    'version-string' = '1'
    'builtin-baseline' = $baseline
    dependencies = @([ordered]@{ name = 'dsqt'; 'default-features' = $false; features = $features })
})
$arguments = @('install', "--vcpkg-root=$VcpkgRoot", "--triplet=$triplet",
    "--x-manifest-root=$manifestRoot", "--overlay-ports=$overlay", "--overlay-triplets=$triplets",
    "--x-install-root=$work/installed", "--x-buildtrees-root=$work/buildtrees",
    "--x-packages-root=$work/packages", "--downloads-root=$work/downloads")
& $vcpkg @arguments
if ($LASTEXITCODE -ne 0) { throw "vcpkg package build failed ($LASTEXITCODE). No ready-package metadata was written." }
$prefix = Join-Path $work "installed/$triplet"
$expected = @('share/dsqt/DsqtConfig.cmake', 'share/dsqt/dsqt-modules.json')
if ('TouchEngine' -in $selectedModules) { $expected += 'bin/TouchEngine.dll', 'debug/bin/TouchEngine.dll' }
foreach ($module in $selectedModules) {
    $expected += "lib/Dsqt$module.lib", "debug/lib/Dsqt$module.lib",
        "lib/Dsqt${module}plugin.lib", "debug/lib/Dsqt${module}plugin.lib"
}
foreach ($file in $expected) {
    if (!(Test-Path -LiteralPath (Join-Path $prefix $file))) { throw "Incomplete package: $file" }
}
Write-Json $metadataPath ([ordered]@{
    formatVersion = 1
    builtModules = $selectedModules
    packageVersion = $packageVersion
    sourceFingerprint = $digest
    qtVersion = $qtVersion
    qtKit = $kit.Replace('\', '/')
    triplet = $triplet
    builtinBaseline = $baseline
})
Write-Host "`nReady: dsqt $packageVersion (Debug and Release)"
Write-Host "Local package: $work"
Write-Host "In a ProjectCloner application, from an x64 MSVC developer prompt:"
Write-Host "  set `"DSQT_LOCAL_PACKAGE_ROOT=$work`""
Write-Host '  cmake --preset ninja-vcpkg-local'
Write-Host '  cmake --build --preset ninja-vcpkg-local-release'
Write-Host 'This is a local source overlay plus installed binaries. No registry was published.'
