<#
.SYNOPSIS
    Builds the dsqt port locally, against this working copy, without publishing
    anything.

.DESCRIPTION
    Uses vcpkg classic mode with an overlay port, so nothing touches the
    registry. The overlay is a temporary copy of Library/vcpkg/port.

    Source selection:

      (default)      builds a copy of your working tree, uncommitted edits
                     included. No git, no network, no LFS. Fast, and the right
                     mode while iterating on CMake.
      -FromRemote    fetches from the real GitHub URL via vcpkg_from_git, the
                     way a consumer will. Exercises the fetch, Git LFS and your
                     credentials. The commit must already be pushed.

    There is deliberately no local file:// mode. DsQt tracks assets in Git LFS,
    and git-lfs has no API over the file:// transport, so vcpkg_from_git cannot
    produce a source archive from a local path.

    Extra checks:

      -CacheTest     removes and reinstalls to prove the second build is served
                     from the binary cache rather than recompiled.
      -CompareQt     installs under two Qt triplets and shows that they land in
                     separate cache entries, i.e. that a Qt kit change cannot
                     silently reuse the wrong binaries.

    NOTE: this file is deliberately pure ASCII. Windows PowerShell 5.1 reads a
    BOM-less .ps1 as the ANSI code page, and UTF-8 punctuation such as an em
    dash decodes to a curly quote there, which PowerShell treats as a string
    delimiter. Keep it ASCII.

.PARAMETER QtVersion
    Qt kit to build against. Must be installed under -QtRoot.

.PARAMETER SecondQtVersion
    Second kit for -CompareQt.

.PARAMETER FromRemote
    Fetch from GitHub instead of building the working tree. Requires the commit
    to be pushed and git-lfs to be installed.

.PARAMETER Commit
    Commit to fetch with -FromRemote. Defaults to the current HEAD.

.EXAMPLE
    .\Test-DsqtPort.ps1 -QtVersion 6.10.2

.EXAMPLE
    .\Test-DsqtPort.ps1 -QtVersion 6.10.2 -CacheTest

.EXAMPLE
    .\Test-DsqtPort.ps1 -QtVersion 6.10.2 -FromRemote

.EXAMPLE
    .\Test-DsqtPort.ps1 -QtVersion 6.10.2 -SecondQtVersion 6.9.3 -CompareQt
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $QtVersion,
    [string] $SecondQtVersion,
    [string] $QtRoot = 'C:\Qt',
    [string] $QtArch = 'msvc2022_64',
    [string] $VcpkgRoot = $env:VCPKG_ROOT,
    [switch] $CacheTest,
    [switch] $CompareQt,
    [switch] $FromRemote,
    [string] $Commit,
    [switch] $StageOnly,
    [switch] $CleanTemp
)

$ErrorActionPreference = 'Stop'

function Info ($m) { Write-Host "  $m" }
function Step ($m) { Write-Host ''; Write-Host "=== $m ===" -ForegroundColor Cyan }
function Warn ($m) { Write-Host "  ! $m" -ForegroundColor Yellow }
function Pass ($m) { Write-Host "  + $m" -ForegroundColor Green }
function Fail ($m) { Write-Host "  x $m" -ForegroundColor Red; exit 1 }

# vcpkg triplet names allow only lowercase alphanumerics and hyphens, so the
# dots in a Qt version are flattened: 6.10.2 -> x64-windows-qt6-10-2.
function Get-TripletName([string] $Version) {
    return 'x64-windows-qt' + ($Version -replace '\.', '-')
}

# --- Locate things ----------------------------------------------------------
Step 'Environment'

if (-not $VcpkgRoot) {
    $cmd = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($cmd) { $VcpkgRoot = Split-Path $cmd.Source -Parent }
}
if (-not $VcpkgRoot -or -not (Test-Path (Join-Path $VcpkgRoot 'vcpkg.exe'))) {
    Fail 'vcpkg not found. Set $env:VCPKG_ROOT or pass -VcpkgRoot.'
}
$vcpkg = Join-Path $VcpkgRoot 'vcpkg.exe'
Info "vcpkg:  $vcpkg"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$portSrc  = Join-Path $PSScriptRoot 'port'
$triplets = Join-Path $PSScriptRoot 'triplets'
Info "repo:   $repoRoot"

$kitPath = Join-Path (Join-Path $QtRoot $QtVersion) $QtArch
if (-not (Test-Path $kitPath)) { Fail "Qt kit not installed: $kitPath" }
Info "Qt kit: $kitPath"

# Preflight: vcpkg's package database and its installed/ tree can drift apart if
# the tree is deleted by hand. vcpkg then plans no dependency work ("already
# installed") while CMake cannot find any of them, which surfaces much later as
# a confusing "Could not find a package configuration file" during configure.
$primaryTriplet = Get-TripletName $QtVersion
$installedDir = Join-Path $VcpkgRoot "installed\$primaryTriplet"
$dbListed = @(@(& $vcpkg list 2>$null) |
    ForEach-Object { ($_ -split '\s+')[0] } |
    Where-Object { $_ -like "*:$primaryTriplet" } |
    ForEach-Object { $_ -replace '\[.*?\]', '' } |
    Select-Object -Unique)

# Check each registered package individually. A whole-tree check is not enough:
# vcpkg can restore a package's direct dependencies while leaving deeper ones
# registered-but-absent, and CMake then silently falls back to a system copy
# (e.g. finding Vulkan in C:\VulkanSDK instead of vcpkg's vulkan-headers). That
# produces binaries the ABI hash does not describe, which is the exact problem
# the Qt-in-the-triplet design exists to avoid.
$vcpkgInstalled = Join-Path $VcpkgRoot 'installed'
$infoDir = Join-Path $vcpkgInstalled 'vcpkg\info'
$stalePkgs = @()
foreach ($spec in $dbListed) {
    $pkgName = ($spec -split ':')[0]
    $pattern = '{0}_*_{1}.list' -f $pkgName, $primaryTriplet
    $lf = Get-ChildItem $infoDir -Filter $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $lf) { continue }
    # Entries are relative to installed/ and directories end with a slash.
    $entries = @(Get-Content -LiteralPath $lf.FullName |
                 Where-Object { $_ -and -not $_.EndsWith('/') } |
                 Select-Object -First 8)
    if ($entries.Count -eq 0) { continue }   # metapackage with no files
    $found = @($entries | Where-Object { Test-Path (Join-Path $vcpkgInstalled $_) })
    if ($found.Count -eq 0) { $stalePkgs += $spec }
}

if ($stalePkgs.Count -gt 0) {
    Warn "$($stalePkgs.Count) package(s) are registered but their files are missing:"
    $stalePkgs | ForEach-Object { Warn "   $_" }
    Warn ''
    Warn 'Deleting installed/<triplet>/ removes the files but leaves the records'
    Warn 'in installed/vcpkg/status, so vcpkg plans no work for them. CMake then'
    Warn 'either fails to find them or silently uses a system copy instead.'
    Warn 'Deleting buildtrees/ does not help -- that is only scratch space.'
    Warn ''
    Warn 'Repair, then re-run this script:'
    Warn ("   vcpkg remove --recurse " + ($stalePkgs -join ' '))
    Warn ''
}

# --- Triplets ---------------------------------------------------------------
Step 'Triplets'
$wanted = @($QtVersion)
if ($CompareQt) {
    if (-not $SecondQtVersion) { Fail '-CompareQt needs -SecondQtVersion.' }
    $wanted += $SecondQtVersion
}
foreach ($v in $wanted) {
    $name = Get-TripletName $v
    $f = Join-Path $triplets "$name.cmake"
    if (-not (Test-Path $f)) {
        Info "generating triplet for $v"
        & (Join-Path $PSScriptRoot 'New-DsqtTriplet.ps1') -QtRoot $QtRoot -Arch $QtArch | Out-Null
    }
    if (-not (Test-Path $f)) { Fail "No triplet for Qt $v (is that kit installed?)" }
    Pass $name
}

# --- Build a temporary overlay port -----------------------------------------
Step 'Overlay port'
# The paths here are deliberately STABLE, not randomised. vcpkg hashes every
# file in the port directory, and working-tree mode writes SOURCE_PATH into
# portfile.cmake -- so a random temp path would change the ABI hash on every
# run and no build could ever be restored from the binary cache.
$temp = Join-Path ([IO.Path]::GetTempPath()) 'dsqt-port-test'
$overlay = Join-Path $temp 'dsqt'
if (Test-Path $overlay) { Remove-Item $overlay -Recurse -Force }
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
Copy-Item (Join-Path $portSrc '*') $overlay -Recurse

$portfile = Join-Path $overlay 'portfile.cmake'
$text = Get-Content $portfile -Raw

if ($FromRemote) {
    $sha = if ($Commit) { $Commit.Trim() } else { (& git -C $repoRoot rev-parse HEAD).Trim() }

    # The commit has to be on the remote, or the fetch inside the port fails.
    & git -C $repoRoot cat-file -e "$sha^{commit}" 2>$null
    if ($LASTEXITCODE -ne 0) { Fail "Not a commit in this repo: $sha" }

    $onRemote = @(& git -C $repoRoot branch -r --contains $sha 2>$null) |
                Where-Object { $_ -and $_.Trim() }
    if (@($onRemote).Count -eq 0) {
        Warn "Commit $($sha.Substring(0,10)) does not appear on any remote branch."
        Warn 'Push it first, or the port will fail to fetch it.'
    }

    if (-not (Get-Command git-lfs -ErrorAction SilentlyContinue)) {
        Warn 'git-lfs was not found on PATH. DsQt tracks assets in LFS and the'
        Warn 'port fetches them, so this run will probably fail.'
    }

    $text = $text -replace 'REF\s+"[0-9a-f]{40}"', ('REF "' + $sha + '"')
    if ($text -notmatch [regex]::Escape($sha)) {
        Fail 'Could not substitute REF; portfile.cmake structure changed.'
    }
    $sourceDesc = 'commit ' + $sha.Substring(0, 10) + ' fetched from GitHub'
}
else {
    Info 'Building the working tree; the vcpkg_from_git path is NOT exercised.'
    Info 'Use -FromRemote to test the real fetch.'
    $staged = Join-Path $temp 'src'
    if (Test-Path $staged) { Remove-Item $staged -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $staged | Out-Null

    # Mirror tracked + untracked files so the port sees uncommitted edits.
    $tracked   = & git -C $repoRoot ls-files
    $untracked = & git -C $repoRoot ls-files --others --exclude-standard
    $files = @($tracked) + @($untracked)
    foreach ($rel in $files) {
        if ([string]::IsNullOrWhiteSpace($rel)) { continue }
        if ($rel -match '(^|/)build/') { continue }
        $src = Join-Path $repoRoot $rel
        if (-not (Test-Path -LiteralPath $src -PathType Leaf)) { continue }
        $dst = Join-Path $staged $rel
        New-Item -ItemType Directory -Force -Path (Split-Path $dst -Parent) | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }

    # vcpkg's ABI hash covers the port directory, not whatever SOURCE_PATH points
    # at. With a stable path and nothing else, editing library source would leave
    # the hash unchanged and vcpkg would restore stale binaries. So fold a digest
    # of the staged sources into portfile.cmake: edits invalidate the cache,
    # an unchanged tree restores from it. Only Library/ is hashed, since that is
    # all the port builds.
    Info 'Hashing staged sources...'
    $manifest = New-Object Text.StringBuilder
    Get-ChildItem (Join-Path $staged 'Library') -Recurse -File |
        Sort-Object FullName |
        ForEach-Object {
            $rel = $_.FullName.Substring($staged.Length).Replace('\', '/')
            # Skip Library/vcpkg/: the port files there are copied into the
            # overlay and hashed by vcpkg itself, and this script lives there
            # too. Including it would make every edit to the tooling invalidate
            # the library's cache and force a full rebuild for no reason.
            if ($rel -match '^[\\/]?Library/vcpkg/') { return }
            $h = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
            [void]$manifest.AppendLine("$rel $h")
        }
    $manifestBytes = [Text.Encoding]::UTF8.GetBytes($manifest.ToString())
    $stream = New-Object IO.MemoryStream(, $manifestBytes)
    $sourceHash = (Get-FileHash -InputStream $stream -Algorithm SHA256).Hash
    $stream.Dispose()

    $srcCmake = $staged -replace '\\', '/'
    $inject = 'set(SOURCE_PATH "' + $srcCmake + '")  # injected by Test-DsqtPort.ps1' + [Environment]::NewLine +
              '# staged-source-sha256: ' + $sourceHash
    $text = [regex]::Replace($text, '(?s)vcpkg_from_git\(.*?\r?\n\)', $inject)
    if ($text -notmatch [regex]::Escape($sourceHash)) {
        Fail 'Could not inject SOURCE_PATH; portfile.cmake structure changed.'
    }
    $sourceDesc = 'working tree, sha256 ' + $sourceHash.Substring(0, 12).ToLower()
}

Set-Content -LiteralPath $portfile -Value $text -Encoding UTF8
Info "source:  $sourceDesc"
Info "overlay: $overlay"

if ($StageOnly) {
    Step 'Staged'
    Pass 'Overlay port refreshed from the working tree; nothing installed.'
    Info 'Point a consuming project at it with:'
    Info "   -DVCPKG_OVERLAY_PORTS=$temp"
    Info "   -DVCPKG_OVERLAY_TRIPLETS=$triplets"
    Info ''
    Info 'Use this when a consumer will build dsqt anyway (a different'
    Info 'builtin-baseline gives it its own ABI hash), so you do not pay for'
    Info 'the same compile twice.'
    exit 0
}

# --- Helpers ----------------------------------------------------------------
function Install-Dsqt([string] $TripletName, [switch] $ForceBuild) {
    $vcpkgArgs = @(
        'install', "dsqt:$TripletName",
        "--overlay-ports=$temp",
        "--overlay-triplets=$triplets",
        '--recurse'
    )
    if ($ForceBuild) {
        # Disable binary cache *reads* so the portfile is actually executed --
        # otherwise a matching ABI hash restores the package and vcpkg_from_git
        # never runs, making a fetch test vacuous. Results are still written
        # back to the cache.
        $vcpkgArgs += '--binarysource=clear;default,write'
    }
    Write-Host ('  > vcpkg ' + ($vcpkgArgs -join ' ')) -ForegroundColor DarkGray

    $sw = [Diagnostics.Stopwatch]::StartNew()
    $output = & $vcpkg @vcpkgArgs 2>&1
    $code = $LASTEXITCODE
    $sw.Stop()

    $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }

    if ($code -ne 0) {
        # vcpkg points at a log file and stops. Print the tail of anything it
        # wrote, since that is where the real error is.
        $logs = @($output | Select-String -Pattern '([A-Za-z]:\\[^\s"]+\.log)' -AllMatches |
                  ForEach-Object { $_.Matches } | ForEach-Object { $_.Value } |
                  Select-Object -Unique)
        foreach ($log in $logs) {
            if (Test-Path -LiteralPath $log) {
                Write-Host ''
                Write-Host "--- $log ---" -ForegroundColor Yellow
                Get-Content -LiteralPath $log -Tail 40 |
                    ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
            }
        }
        Fail "vcpkg install failed for $TripletName (exit $code)"
    }

    $joined = ($output | Out-String)

    # vcpkg prints each package's ABI hash. Capture dsqt's specifically: it is
    # the cache key, so it is the thing worth comparing across triplets.
    $abi = ''
    if ($joined -match 'dsqt[^\r\n]*?package ABI:\s*([0-9a-f]{16,})') {
        $abi = $Matches[1]
    }

    # Three outcomes worth telling apart, and "Restored N package(s)" tells
    # them apart badly -- it counts dependencies, not dsqt:
    #   Built           vcpkg compiled the port (portfile ran)
    #   AlreadyInstalled  vcpkg did nothing at all; installed state is tracked
    #                     by name+triplet, not ABI, so a stale package short-
    #                     circuits the whole install and verifies nothing
    #   otherwise       restored from the binary cache
    $built = $joined -match 'Building dsqt'
    $already = $joined -match 'The following packages are already installed'

    [pscustomobject]@{
        Triplet          = $TripletName
        Seconds          = [math]::Round($sw.Elapsed.TotalSeconds, 1)
        Built            = $built
        AlreadyInstalled = ($already -and -not $built)
        FromCache        = (-not $built -and -not $already)
        Abi              = $abi
    }
}

function Remove-Dsqt([string] $TripletName) {
    & $vcpkg remove "dsqt:$TripletName" "--overlay-ports=$temp" "--overlay-triplets=$triplets" 2>&1 |
        Out-Null
}

try {
    $primary = Get-TripletName $QtVersion

    Step "Install ($primary)"
    # Always remove first. vcpkg considers a package installed by name+triplet
    # alone, so leaving a stale one makes the install a no-op that verifies
    # nothing -- the run looks like a fast cache hit but never touches the port.
    Remove-Dsqt $primary
    $r1 = Install-Dsqt $primary -ForceBuild:$FromRemote

    if ($r1.AlreadyInstalled) {
        Warn ('nothing was installed (' + $r1.Seconds + 's) -- vcpkg considered it already present')
        Warn 'This run verified nothing. Remove dsqt and try again.'
    }
    elseif ($r1.Built) {
        Pass ('built from source in ' + $r1.Seconds + 's')
    }
    else {
        Pass ('restored from binary cache in ' + $r1.Seconds + 's')
    }

    if ($FromRemote -and -not $r1.Built) {
        Warn 'The port was not compiled, so vcpkg_from_git did NOT run and the'
        Warn 'fetch/LFS path is still unverified.'
    }

    # --- Verify the installed layout ----------------------------------------
    Step 'Installed layout'
    $prefix = Join-Path $VcpkgRoot "installed\$primary"

    # Check what dsqt OWNS, not what happens to be in the prefix. installed/ is
    # shared with every other port -- vulkan-headers alone puts ~100 headers in
    # include/ -- so a filesystem count there proves nothing about dsqt.
    # vcpkg records each port's files in installed/vcpkg/info/<port>_<ver>_<triplet>.list,
    # with triplet-relative paths and directories listed with a trailing slash.
    $infoDir = Join-Path $VcpkgRoot 'installed\vcpkg\info'
    $listFile = $null
    if (Test-Path $infoDir) {
        $listFile = Get-ChildItem $infoDir -Filter "dsqt_*_$primary.list" -ErrorAction SilentlyContinue |
                    Select-Object -First 1
    }
    $owned = @()
    if ($listFile) {
        $owned = @(Get-Content -LiteralPath $listFile.FullName |
                   ForEach-Object { $_ -replace "^$([regex]::Escape($primary))/", '' } |
                   Where-Object { $_ })
        Info "manifest: $($listFile.Name) ($($owned.Count) entries)"
    }
    else {
        Warn 'No vcpkg file manifest found; falling back to filesystem checks.'
    }

    function Test-Owned([string] $RelPath) {
        if (-not $listFile) { return (Test-Path (Join-Path $prefix $RelPath)) }
        return $owned -contains ($RelPath -replace '\\', '/')
    }
    $checks = @(
        @{ P = 'share\dsqt\DsqtConfig.cmake';        D = 'CMake package config' },
        @{ P = 'share\dsqt\DsqtConfigVersion.cmake'; D = 'version file' },
        @{ P = 'share\dsqt\usage';                   D = 'usage message' },
        @{ P = 'share\dsqt\copyright';               D = 'copyright' },
        @{ P = 'share\dsqt\qml\Dsqt\Core\qmldir';    D = 'QML module metadata' },
        @{ P = 'lib\DsqtCore.lib';                   D = 'release Core' },
        @{ P = 'debug\lib\DsqtCore.lib';             D = 'debug Core' },
        @{ P = 'lib\DsqtCoreplugin.lib';             D = 'release Core QML plugin' },
        # A genuinely public header. Note dsVersion.h is NOT a valid canary:
        # it is generated into the build tree and only included by a .cpp, so
        # it is deliberately never installed.
        @{ P = 'include\core\dsGuiApplication.h';    D = 'Core headers' }
    )
    $missing = 0
    foreach ($c in $checks) {
        if (Test-Owned $c.P) { Pass ($c.D + '  (' + $c.P + ')') }
        else { Warn ('MISSING ' + $c.D + '  (' + $c.P + ')'); $missing++ }
    }
    if ($missing -gt 0) { Warn "$missing expected path(s) missing; see Docs/vcpkg.md" }

    if ($listFile) {
        # Headers dsqt itself installed. The source tree has ~101, so anything
        # far below that means the install(DIRECTORY) rules missed a module.
        $ownedHeaders = @($owned | Where-Object { $_ -match '^include/.+\.(h|hpp)$' })
        if ($ownedHeaders.Count -ge 90) {
            Pass "$($ownedHeaders.Count) headers installed by dsqt"
        }
        else {
            Warn "dsqt installed only $($ownedHeaders.Count) headers (expected ~101)"
        }

        # Directory entries end with '/'. One is empty if no owned file sits
        # beneath it. The portfile prunes these; survivors mean vcpkg would
        # reject the package.
        $ownedFiles = @($owned | Where-Object { -not $_.EndsWith('/') })
        $emptyOwned = @($owned | Where-Object { $_.EndsWith('/') } | Where-Object {
            $d = $_
            -not ($ownedFiles | Where-Object { $_.StartsWith($d) })
        })
        if ($emptyOwned.Count -eq 0) { Pass 'no empty directories installed' }
        else {
            Warn "$($emptyOwned.Count) empty director(ies) installed:"
            $emptyOwned | Select-Object -First 5 | ForEach-Object { Info "   $_" }
        }
    }

    # Every library dsqt installs must carry the Dsqt prefix, or it risks
    # colliding with another port in the shared prefix.
    if ($listFile) {
        $unprefixed = @($owned |
            Where-Object { $_ -match '^(debug/)?lib/([^/]+)\.lib$' } |
            ForEach-Object { $Matches[2] } |
            Where-Object { -not $_.StartsWith('Dsqt') } |
            Select-Object -Unique)
        if ($unprefixed.Count -eq 0) { Pass 'all installed libraries are Dsqt-prefixed' }
        else {
            Warn "unprefixed librar(ies) -- collision risk: $($unprefixed -join ', ')"
        }
    }

    # toml++ should be static, so no DLL anywhere in the tree.
    $tomlDll = Get-ChildItem $prefix -Recurse -Filter 'tomlplusplus*.dll' -ErrorAction SilentlyContinue
    if ($tomlDll) { Warn ('toml++ built as a DLL: ' + $tomlDll[0].FullName) }
    else { Pass 'toml++ linked statically (no tomlplusplus DLL in the tree)' }

    # --- Cache test ---------------------------------------------------------
    if ($CacheTest) {
        Step 'Binary cache'
        Remove-Dsqt $primary
        $r2 = Install-Dsqt $primary
        if ($r2.FromCache) {
            Pass ('second install restored from cache in ' + $r2.Seconds +
                  's (first: ' + $r1.Seconds + 's)')
        }
        else {
            Warn ('second install did NOT hit the cache (' + $r2.Seconds +
                  's); something in the ABI hash is unstable')
        }
    }

    # --- Two Qt kits must not share a cache entry ---------------------------
    if ($CompareQt) {
        Step 'Qt kit isolation'
        $secondary = Get-TripletName $SecondQtVersion
        $r3 = Install-Dsqt $secondary
        if ($r3.FromCache) {
            Warn ($secondary + ' was restored from cache; it must NOT share an entry with ' + $primary)
        }
        else {
            Pass ($secondary + ' compiled separately (' + $r3.Seconds + 's), as it should')
        }

        # The ABI hash *is* the cache key, so comparing the two directly is the
        # real test -- far more direct than eyeballing archive filenames.
        if ($r1.Abi -and $r3.Abi) {
            Info ''
            Info "ABI hash for $($primary):"
            Info "   $($r1.Abi)"
            Info "ABI hash for $($secondary):"
            Info "   $($r3.Abi)"
            if ($r1.Abi -eq $r3.Abi) {
                Warn 'IDENTICAL -- the Qt kit is not reaching the ABI hash.'
                Warn 'Binaries built against one kit would be reused for the other.'
            }
            else {
                Pass 'distinct ABI hashes: each Qt kit gets its own cache entry'
            }
        }

        # Honour VCPKG_DEFAULT_BINARY_CACHE; %LOCALAPPDATA%\vcpkg\archives is
        # only the default when that is unset.
        $archives = $env:VCPKG_DEFAULT_BINARY_CACHE
        if (-not $archives) { $archives = Join-Path $env:LOCALAPPDATA 'vcpkg\archives' }
        if (Test-Path $archives) {
            Info ''
            Info "Most recent entries in $archives"
            Get-ChildItem $archives -Recurse -Filter '*.zip' -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 6 |
                ForEach-Object { Info ('   ' + $_.Name) }
        }
        else {
            Info "Binary cache directory not found: $archives"
        }
    }

    Step 'Done'
    Pass "dsqt installed at $prefix"
    Info 'Point a consuming project at it with:'
    Info "   -DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake"
    Info "   -DVCPKG_TARGET_TRIPLET=$primary"
    Info "   -DVCPKG_OVERLAY_TRIPLETS=$triplets"
}
finally {
    # Deliberately NOT deleted. The overlay and staging directories are reused
    # at a fixed path so the ABI hash stays stable between runs; each run wipes
    # and repopulates them at the start. Delete by hand if you want the space
    # back -- the next run just re-stages.
    if ($CleanTemp -and (Test-Path $temp)) {
        Remove-Item $temp -Recurse -Force -ErrorAction SilentlyContinue
        Info "removed $temp"
    }
    else {
        Info "working files: $temp"
    }
}
