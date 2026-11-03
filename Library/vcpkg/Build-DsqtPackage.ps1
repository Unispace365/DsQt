<#
.SYNOPSIS
Build a local DsQt source package for ProjectCloner applications.
.DESCRIPTION
Uses the Library manifest's dependency baseline and a snapshot of the working
tree. Produces Debug and Release together, using the caller's binary cache.
Publishes the completed source package under LOCALAPPDATA/DsQt/vcpkg and
updates its local default. Does not publish to the GitHub registry.
Run directly from an x64 MSVC developer shell, or through build_and_install.bat.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Qt,
    [string] $VcpkgRoot = $env:VCPKG_ROOT,
    [string] $PackageRoot,
    [string] $PublishRoot,
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
if (!$PublishRoot) {
    if (!$env:LOCALAPPDATA) { throw 'LOCALAPPDATA is unavailable. Supply -PublishRoot.' }
    $PublishRoot = Join-Path $env:LOCALAPPDATA 'DsQt/vcpkg'
}
$store = [IO.Path]::GetFullPath($PublishRoot).TrimEnd('\', '/')
if ($store -eq $work -or $store.StartsWith($work + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $work.StartsWith($store + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $store -eq $VcpkgRoot -or $store.StartsWith($VcpkgRoot.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'PublishRoot must be separate from PackageRoot and VCPKG_ROOT.'
}
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
$portfilePath = Join-Path $overlay 'dsqt/portfile.cmake'
# Build and publish exactly the same recipe. A checkout-specific SOURCE_PATH
# would break the published copy and change its vcpkg binary-cache identity.
$portfile = [IO.File]::ReadAllText($portfilePath)
$stagedSource = 'set(SOURCE_PATH "' + (Join-Path $work 'source').Replace('\', '/') + '")'
if (!$portfile.Contains($stagedSource)) { throw 'The staged source path was not found in the port.' }
$portfile = $portfile.Replace($stagedSource, 'set(SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/../../source")')
[IO.File]::WriteAllText($portfilePath, $portfile, (New-Object Text.UTF8Encoding($false)))
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

# Publish only the inputs consumers need. Compiled binaries remain in the
# caller's vcpkg binary cache; buildtrees/downloads stay in the staging area.
# Never replace a package directory already selected by another application.
New-Item -ItemType Directory -Force -Path $store | Out-Null
$publicationLock = [IO.File]::Open((Join-Path $store 'publication.lock'),
    [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
try {
    $packages = Join-Path $store 'packages'
    New-Item -ItemType Directory -Force -Path $packages | Out-Null
    $publicationId = [Guid]::NewGuid().ToString('N')
    $pending = [IO.Path]::GetFullPath((Join-Path $packages ".staging-$publicationId"))
    $published = [IO.Path]::GetFullPath((Join-Path $packages "$packageVersion-$publicationId"))
    foreach ($path in @($pending, $published)) {
        if (!$path.StartsWith($store + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Publication path escapes PublishRoot: $path"
        }
    }
    New-Item -ItemType Directory -Path $pending | Out-Null
    foreach ($name in @('source', 'overlay', 'triplets')) {
        Copy-Item -LiteralPath (Join-Path $work $name) -Destination $pending -Recurse
    }
    Copy-Item -LiteralPath $metadataPath -Destination $pending
    # Check the copied inputs before exposing this publication to consumers.
    foreach ($name in @('source/Library/CMakeLists.txt', 'source/Library/dsqt-modules.json',
                        'overlay/dsqt/portfile.cmake', 'overlay/dsqt/vcpkg.json',
                        "triplets/$triplet.cmake", 'dsqt-local-package.json')) {
        if (!(Test-Path -LiteralPath (Join-Path $pending $name) -PathType Leaf)) {
            throw "Incomplete local publication: $name"
        }
    }
    [IO.Directory]::Move($pending, $published)
    $defaultPath = Join-Path $store 'dsqt-local-default.json'
    $nextDefault = Join-Path $store "default-$publicationId.tmp"
    Write-Json $nextDefault ([ordered]@{
        formatVersion = 1
        packageRoot = $published.Replace('\', '/')
    })
    if (Test-Path -LiteralPath $defaultPath) {
        [IO.File]::Replace($nextDefault, $defaultPath, (Join-Path $store 'dsqt-local-default.previous.json'))
    } else {
        [IO.File]::Move($nextDefault, $defaultPath)
    }
} finally {
    $publicationLock.Dispose()
}
Write-Host "`nReady: dsqt $packageVersion (Debug and Release)"
Write-Host "Local package: $published"
Write-Host "Local default: $store"
Write-Host "In a ProjectCloner application, from an x64 MSVC developer prompt:"
if ($env:LOCALAPPDATA -and $store -eq [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA 'DsQt/vcpkg'))) {
    Write-Host '  rem The standard LOCALAPPDATA/DsQt/vcpkg store is discovered automatically.'
} else {
    Write-Host "  set `"DSQT_LOCAL_PACKAGE_ROOT=$store`""
}
Write-Host "  rem To select this exact package: set `"DSQT_LOCAL_PACKAGE_ROOT=$published`""
Write-Host '  cmake --fresh --preset ninja-vcpkg-local'
Write-Host '  cmake --build --preset ninja-vcpkg-local-release'
Write-Host 'Published locally. Consumers reuse the binary cache or build from this source package. No GitHub registry was changed.'
