<#
.SYNOPSIS
Build and validate the Waffles source port without publishing it.
.DESCRIPTION
The default mode snapshots tracked and non-ignored source files, including
uncommitted edits. -FromRemote instead exercises the pinned Git/LFS fetch.
All installed packages and staging/build files stay under WorkRoot. This
script never removes packages from the caller's ordinary vcpkg installation.
Run from an x64 MSVC developer shell matching the selected Qt kit.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $QtVersion,
    [string] $QtRoot = 'C:\Qt',
    [ValidateSet('msvc2022_64')] [string] $QtArch = 'msvc2022_64',
    [string] $VcpkgRoot = $env:VCPKG_ROOT,
    [string] $WorkRoot = (Join-Path $PSScriptRoot '../build/vcpkg-validation/port-test'),
    [switch] $StageOnly,
    [switch] $CacheTest,
    [switch] $FromRemote,
    [string] $Commit
)
$ErrorActionPreference = 'Stop'
if ($CacheTest -and $FromRemote) { throw 'Use working-tree mode for -CacheTest; remote mode deliberately bypasses cache reads.' }
if ([version]$QtVersion -lt [version]'6.12.0') { throw 'Waffles requires Qt 6.12 or newer.' }
if (-not $VcpkgRoot) { throw 'Set VCPKG_ROOT or pass -VcpkgRoot.' }
$vcpkg = Join-Path $VcpkgRoot 'vcpkg.exe'
if (!(Test-Path -LiteralPath $vcpkg)) { throw "Missing vcpkg: $vcpkg" }
$kit = Join-Path (Join-Path $QtRoot $QtVersion) $QtArch
foreach ($file in @('bin/qmake.exe', 'lib/cmake/Qt6/Qt6Config.cmake',
                    'lib/cmake/Qt6CanvasPainter/Qt6CanvasPainterConfig.cmake',
                    'lib/cmake/Qt6PdfQuick/Qt6PdfQuickConfig.cmake')) {
    if (!(Test-Path -LiteralPath (Join-Path $kit $file))) { throw "Incomplete Qt kit: $kit/$file" }
}
if (!$StageOnly -and !(Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'Run from the matching x64 MSVC developer shell.'
}
if (!$StageOnly -and $env:VSCMD_ARG_TGT_ARCH -ne 'x64') { throw 'Select the x64 MSVC environment.' }
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$work = [IO.Path]::GetFullPath($WorkRoot)
if ($work.TrimEnd('\', '/') -eq [IO.Path]::GetFullPath($VcpkgRoot).TrimEnd('\', '/')) {
    throw 'WorkRoot must be separate from the vcpkg installation.'
}
$repoPath = [IO.Path]::GetFullPath($repo).TrimEnd('\', '/')
if ($work.TrimEnd('\', '/') -eq $repoPath -or $repoPath.StartsWith($work.TrimEnd('\', '/') + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'WorkRoot must not be the repository or an ancestor of it.'
}
New-Item -ItemType Directory -Force -Path $work | Out-Null
function Reset-WorkDirectory([string] $Name) {
    $path = [IO.Path]::GetFullPath((Join-Path $work $Name))
    if (!$path.StartsWith($work.TrimEnd('\', '/') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to reset a path outside WorkRoot: $path"
    }
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $path | Out-Null
    return $path
}
$overlay = Reset-WorkDirectory 'overlay/dsqt'
Copy-Item -Path (Join-Path $PSScriptRoot 'port/*') -Destination $overlay
$portfile = Join-Path $overlay 'portfile.cmake'
$text = Get-Content -LiteralPath $portfile -Raw -Encoding UTF8
if ($FromRemote) {
    if (!(Get-Command git-lfs -ErrorAction SilentlyContinue)) { throw 'Remote source validation requires Git LFS.' }
    $ref = if ($Commit) { $Commit } else { 'HEAD' }
    $sha = & git -C $repo rev-parse --verify "$ref^{commit}"
    if ($LASTEXITCODE -ne 0) { throw "Invalid commit: $ref" }
    $sha = $sha.Trim()
    $text = $text -replace 'REF "[0-9a-f]{40}"', ('REF "' + $sha + '"')
} else {
    $source = Reset-WorkDirectory 'source'
    $tracked = @(& git -C $repo -c core.quotepath=false ls-files)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot list tracked source files.' }
    $untracked = @(& git -C $repo -c core.quotepath=false ls-files --others --exclude-standard)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot list untracked source files.' }
    foreach ($rel in ($tracked + $untracked | Sort-Object -Unique)) {
        if (!$rel -or $rel -notlike 'Library/*' -or $rel -match '(^|/)build/') { continue }
        $inputFile = Join-Path $repo $rel
        if (!(Test-Path -LiteralPath $inputFile -PathType Leaf)) { continue }
        $outputFile = Join-Path $source $rel
        New-Item -ItemType Directory -Force -Path (Split-Path $outputFile) | Out-Null
        Copy-Item -LiteralPath $inputFile -Destination $outputFile
    }
    $hashes = @(Get-ChildItem -LiteralPath (Join-Path $source 'Library') -File -Recurse |
        Sort-Object FullName | ForEach-Object {
            $rel = $_.FullName.Substring($source.Length).Replace('\', '/')
            if ($rel -notlike '/Library/vcpkg/*') { "$rel $((Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash)" }
        })
    $sha256 = [Security.Cryptography.SHA256]::Create()
    try { $digest = [BitConverter]::ToString($sha256.ComputeHash([Text.Encoding]::UTF8.GetBytes(($hashes -join "`n")))).Replace('-', '').ToLowerInvariant() }
    finally { $sha256.Dispose() }
    $injection = 'set(SOURCE_PATH "' + $source.Replace('\', '/') + '")' + "`n# staged-source-sha256: $digest"
    $text = [regex]::Replace($text, '(?s)vcpkg_from_git\(.*?\r?\n\)', $injection)
    if (!$text.Contains($digest)) { throw 'Failed to select working-tree sources.' }
}
[IO.File]::WriteAllText($portfile, $text.Replace("`r`n", "`n"), (New-Object Text.UTF8Encoding($false)))
$triplet = 'x64-windows-qt' + $QtVersion.Replace('.', '-')
$triplets = Join-Path $work 'triplets'
& (Join-Path $PSScriptRoot 'New-DsqtTriplet.ps1') -QtRoot $QtRoot -Arch $QtArch -QtVersion $QtVersion -OutputDirectory $triplets
if (!(Test-Path -LiteralPath (Join-Path $triplets "$triplet.cmake"))) { throw 'Triplet generation failed.' }
Write-Host "Overlay: $overlay"
Write-Host "Triplet: $triplet"
if ($StageOnly) { return }
$common = @("--vcpkg-root=$VcpkgRoot", '--classic', "--overlay-ports=$overlay", "--overlay-triplets=$triplets",
    "--x-install-root=$work/installed", "--x-buildtrees-root=$work/buildtrees",
    "--x-packages-root=$work/packages", "--downloads-root=$work/downloads")
function Install-Port {
    $arguments = @('install', "dsqt:$triplet") + $common
    if ($FromRemote) { $arguments += '--binarysource=clear;default,write' }
    $output = @(& $vcpkg @arguments 2>&1)
    $code = $LASTEXITCODE
    $output | ForEach-Object { Write-Host $_ }
    if ($code -ne 0) { throw "vcpkg install failed: $code" }
    return ($output -join "`n")
}
# Classic mode tracks installed state by name/triplet. Remove only our isolated
# package so edits to the overlay cannot be hidden by an already-installed hit.
& $vcpkg remove "dsqt:$triplet" @common
if ($LASTEXITCODE -ne 0) { throw 'Failed to reset the isolated package before validation.' }
$first = Install-Port
$prefix = Join-Path $work "installed/$triplet"
$manifest = @(Get-ChildItem -LiteralPath "$work/installed/vcpkg/info" -Filter "dsqt_*_$triplet.list")
if ($manifest.Count -ne 1) { throw 'Expected exactly one installed dsqt package manifest.' }
$owned = @(Get-Content -LiteralPath $manifest[0].FullName | ForEach-Object { $_ -replace "^$([regex]::Escape($triplet))/", '' })
$expected = @('share/dsqt/DsqtConfig.cmake', 'share/dsqt/DsqtConfigVersion.cmake',
    'include/core/dsGuiApplication.h', 'include/qsys/dsQsys.h',
    'bin/TouchEngine.dll', 'debug/bin/TouchEngine.dll',
    'lib/DsqtTouchEngineSDK.lib', 'debug/lib/DsqtTouchEngineSDK.lib')
foreach ($module in @('Core', 'Bridge', 'Waffles', 'Qsys', 'Touch', 'TouchEngine', 'Spout')) {
    $expected += "lib/Dsqt$module.lib", "debug/lib/Dsqt$module.lib", "lib/Dsqt${module}plugin.lib", "debug/lib/Dsqt${module}plugin.lib"
    $expected += "share/dsqt/qml/Dsqt/$module/qmldir"
}
foreach ($rel in $expected) {
    if ($rel -notin $owned -or !(Test-Path -LiteralPath (Join-Path $prefix $rel))) { throw "Missing package-owned file: $rel" }
}
if (Get-ChildItem -LiteralPath $prefix -Recurse -Filter 'tomlplusplus*.dll') { throw 'Expected static toml++ for the package triplet.' }
if ($CacheTest) {
    & $vcpkg remove "dsqt:$triplet" @common
    if ($LASTEXITCODE -ne 0) { throw 'Failed to remove the isolated test package.' }
    $second = Install-Port
    if ($second -match 'Building dsqt' -or $second -notmatch 'Restored \d+ package') { throw 'Second installation did not restore the binary cache.' }
}
Write-Host "Validated dsqt package: $prefix"
