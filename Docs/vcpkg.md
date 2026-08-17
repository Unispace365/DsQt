# Using DsQt with vcpkg

DsQt is published to the private Unispace365 vcpkg registry as a **source
port**. Consumers do not download prebuilt binaries — vcpkg compiles DsQt on
their machine with their compiler and their Qt kit, then caches the result so
the same combination is never built twice.

---

## Why source, and not prebuilt binaries

A static C++/Qt library is only linkable by a build that matches it exactly:
same MSVC toolset, same C runtime, same Qt version down to the patch release.
Qt's moc/qmltype metadata makes this stricter still. A prebuilt DsQt would
either force every project onto one Qt kit or fail at link time in ways that
are miserable to diagnose.

So the port builds from source. The cost of that — compile time — is paid once
per combination, because of the caching described below.

---

## What consumers need to set up

### 0. Prerequisites

- **Git LFS.** DsQt tracks `*.svg`, `*.png`, `*.jpg` and other assets in LFS.
  The port fetches them, so `git-lfs` must be on `PATH`. Anyone who can already
  clone DsQt has it; on a fresh CI runner, install it explicitly.
- A Qt kit from the Qt online installer (see step 3).

### 1. Point vcpkg at the registry

`vcpkg-configuration.json`, next to your `vcpkg.json`:

```json
{
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "4b77da7fed37817f124936239197833469f1b9a8"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/Unispace365/vcpkg-registry",
      "baseline": "<commit sha of the registry's main branch>",
      "packages": [ "dsqt" ]
    }
  ],
  "overlay-triplets": [ "./triplets" ]
}
```

### 2. Depend on dsqt

`vcpkg.json`:

```json
{
  "name": "my-app",
  "version": "1.0.0",
  "dependencies": [
    { "name": "dsqt", "version>=": "0.1.0.42" }
  ]
}
```

### 3. Generate a triplet for your Qt kit

```powershell
# from a DsQt checkout
.\Library\vcpkg\New-DsqtTriplet.ps1 -OutputDirectory C:\dev\MyApp\triplets
```

This writes one triplet per Qt kit installed under `C:\Qt`. Note the naming:
vcpkg only allows lowercase alphanumerics and hyphens in a triplet name, so the
dots in the Qt version are flattened — Qt **6.10.2** becomes
**`x64-windows-qt6-10-2`**. (`DSQT_QT_VERSION` inside the file keeps the real
dotted version.) Commit the triplets alongside your project.

### 4. Build

```powershell
cmake --preset ninja -DVCPKG_TARGET_TRIPLET=x64-windows-qt6-10-2
cmake --build --preset ninja-release
```

Then in `CMakeLists.txt`:

```cmake
find_package(Dsqt CONFIG REQUIRED)
target_link_libraries(myapp PRIVATE Dsqt::Dsqt)
dsqt_deploy_runtime_dlls(myapp)   # copies TouchEngine.dll next to the exe
```

---

## How the caching works

vcpkg keys its binary cache on an **ABI hash**. If the hash matches, the
compiled package is restored from the cache instead of rebuilt. The hash
covers, among other things:

- every file in the port directory — which includes the pinned commit SHA, so
  **the DsQt version** is in the hash;
- the C and C++ compiler executables — so **the compiler** is in the hash;
- the **text of the triplet file**;
- the ABI hash of every dependency.

Qt is the problem case. It is installed by the Qt online installer, not by
vcpkg, so vcpkg has no idea it exists and nothing about it would reach the
hash. Left alone, you would build against Qt 6.9, switch your project to 6.10,
and vcpkg would cheerfully hand you back the 6.9 binaries.

The fix is that **the Qt version is written into the triplet file**:

```cmake
set(DSQT_QT_VERSION "6.10.2")
```

Because the triplet's text is hashed, each Qt kit gets its own triplet, its own
hash, and its own cache entry. The portfile refuses to build if
`DSQT_QT_VERSION` is unset, rather than silently producing a build that would
be cached under a Qt-agnostic key.

So the cache is keyed on exactly what you asked for:

> **DsQt version × compiler × Qt kit**

The local cache is on by default at `%LOCALAPPDATA%\vcpkg\archives`. Nothing
needs configuring. First build of a given combination compiles; every build
after that unpacks in seconds, including from a clean build directory or a
fresh clone.

### Sharing the cache across the team

Optional, but it means only the first person to use a combination pays for it.
Set `VCPKG_BINARY_SOURCES`, for example against GitHub Packages:

```powershell
$env:VCPKG_BINARY_SOURCES = "clear;default,readwrite;nuget,https://nuget.pkg.github.com/Unispace365/index.json,readwrite"
```

---

## Authentication

DsQt is a private repository. The port uses `vcpkg_from_git`, which shells out
to `git`, so it uses whatever credentials the machine already has — Git
Credential Manager on a developer box. **If you can `git clone` DsQt, the port
will build.** No PAT or environment variable to set.

In CI, authenticate git the usual way before invoking vcpkg:

```yaml
- run: git config --global url."https://x-access-token:${{ secrets.GH_PAT }}@github.com/".insteadOf "https://github.com/"
```

---

## How publishing works

Pushing to `ph/develop` triggers `.github/workflows/publish-vcpkg-port.yml`,
which:

1. reads `Library/VERSION.txt` and appends the run number → `0.1.0.42`;
2. copies `Library/vcpkg/port/` into the registry and pins `REF` to the exact
   commit that was pushed;
3. refuses to continue if that version already exists;
4. commits, runs `vcpkg x-add-version`, and pushes.

Nothing is compiled during publishing — the port only records *where* the
source is.

The port files live in **`Library/vcpkg/port/`** in this repo, not in the
registry. The registry copy is generated. Edit them here.

To release a new minor/major version, bump `Library/VERSION.txt`; the run
number continues to increment underneath it.

---

## Testing the port before publishing

Nothing here needs `ph/develop` to exist or the registry to be touched. vcpkg's
*classic mode* with an overlay port builds `dsqt` straight from your working
copy.

### The quick way

```powershell
cd D:\Projects\ds_qt
.\Library\vcpkg\Test-DsqtPort.ps1 -QtVersion 6.10.2
```

By default this builds a copy of your **working tree** — uncommitted edits
included, no git, no network. It copies `Library/vcpkg/port` to a temp
directory, injects a `SOURCE_PATH` in place of the fetch, installs, then checks
the tree has the layout `DsqtConfig.cmake` expects and that toml++ came out
static. This is the right mode while iterating on CMake.

To exercise the real fetch the way a consumer will — `vcpkg_from_git`, Git LFS
and your GitHub credentials — push the commit and run:

```powershell
.\Library\vcpkg\Test-DsqtPort.ps1 -QtVersion 6.10.2 -FromRemote
```

Do that at least once before publishing.

There is deliberately **no local `file://` mode**. DsQt tracks assets in Git
LFS, and git-lfs has no API over the `file://` transport, so `vcpkg_from_git`
cannot build a source archive from a local path.

### Proving the caching actually works

This is the claim worth testing, since it is the whole point:

```powershell
# Build, wipe, rebuild — the second one must come from the cache
.\Library\vcpkg\Test-DsqtPort.ps1 -QtVersion 6.10.2 -CacheTest

# Two Qt kits must NOT share a cache entry
.\Library\vcpkg\Test-DsqtPort.ps1 -QtVersion 6.10.2 -SecondQtVersion 6.9.3 -CompareQt
```

`-CacheTest` passes when the reinstall reports `Restored 1 package(s)` and
finishes in seconds. `-CompareQt` passes when the second kit *does* recompile —
a cache hit there would mean Qt is not reaching the ABI hash, which is the bug
this whole design exists to prevent.

### Doing it by hand

```powershell
$env:VCPKG_ROOT = 'C:\vcpkg'

# Copy the port and pin it to a pushed commit
$tmp = "$env:TEMP\dsqt-overlay"; mkdir "$tmp\dsqt" -Force
copy Library\vcpkg\port\* "$tmp\dsqt"
$sha = git rev-parse HEAD
(Get-Content "$tmp\dsqt\portfile.cmake" -Raw) `
  -replace 'REF\s+"[0-9a-f]{40}"', "REF `"$sha`"" |
  Set-Content "$tmp\dsqt\portfile.cmake"

# Build it
& "$env:VCPKG_ROOT\vcpkg.exe" install dsqt:x64-windows-qt6-10-2 `
    --overlay-ports=$tmp `
    --overlay-triplets=Library\vcpkg\triplets `
    --recurse
```

To see the ABI hash inputs, add `--debug` and look for `<abientries>`.

### Checking you didn't break the existing build

The vcpkg work touched `Library/CMakeLists.txt`, which the normal flow shares.
Confirm the split-config path still installs as it did:

```powershell
cd Library
cmake --preset ninja
cmake --build --preset ninja-release
cmake --install build\ninja --config Release
# expect lib\Release\Core.lib and lib\cmake\Dsqt\DsqtConfig.cmake
```

### Testing the publish workflow

It has `workflow_dispatch`, so you can run it from the Actions tab against a
branch without pushing to `ph/develop`. It will publish a real version to the
registry, so do it deliberately. The duplicate-version guard will stop a second
run at the same version.

---

## Layouts, and why `DsqtConfig.cmake` probes for one

DsQt can be installed two ways, and `find_package(Dsqt)` works with either:

| Layout | Libraries | CMake config | Produced by |
|---|---|---|---|
| split-config | `lib/Debug/`, `lib/Release/` | `lib/cmake/Dsqt` | `build_and_install.bat`, Ninja Multi-Config |
| vcpkg | `lib/`, `debug/lib/` | `share/dsqt` | the vcpkg port |

vcpkg builds each configuration into its own prefix, so the per-config
subdirectories DsQt normally uses would nest redundantly. The port passes
`-DDSQT_INSTALL_CONFIG_SUBDIR=OFF` and friends; `DsqtConfig.cmake` detects at
`find_package()` time which layout it is sitting in and sets the imported
targets' per-config locations accordingly. Existing non-vcpkg workflows are
unaffected.

---

## Troubleshooting

**"the triplet ... does not set DSQT_QT_VERSION"**
You are building with a stock triplet like `x64-windows`. Generate a DsQt
triplet (step 3) and pass `-DVCPKG_TARGET_TRIPLET=x64-windows-qt6-10-2`.

**"Could not find a configuration file for package Qt6 compatible with version 6.9"**
...while pointing at a 6.10 kit. Despite the wording this is usually a
*toolchain* mismatch, not a version one: Qt rejects its own kit when the
compiler differs. `-G Ninja` with no explicit compiler happily picks up
`clang++.exe` from Visual Studio's LLVM directory. Add:

```
-DCMAKE_C_COMPILER=cl -DCMAKE_CXX_COMPILER=cl
```

`DsqtConfig.cmake` also checks this directly and reports the real cause.

**"Compiler mismatch"**
DsQt ships static libraries, so the consuming project must use the compiler the
port was built with. `DSQT_ALLOW_COMPILER_MISMATCH=ON` downgrades it to a
warning.

**dsqt rebuilds in a consuming project even though it is cached**
Check the dependency versions. A consumer's `builtin-baseline` pins its own
versions of `tomlplusplus`, `vulkan-headers` and so on, and those feed into
dsqt's ABI hash. A different baseline is genuinely a different build, so it
gets its own cache entry. For a team to share cached dsqt builds, they need to
share a baseline.

**"Qt version mismatch"**
`DsqtConfig.cmake` records the Qt version the port was built against and refuses
to configure if your `CMAKE_PREFIX_PATH` points at a different kit. DsQt links
Qt private APIs, which have no ABI promise even across patch releases, so this
must match exactly. Either point at the kit named in the error, or rebuild
against yours by selecting the matching triplet. `DSQT_ALLOW_QT_VERSION_MISMATCH=ON`
downgrades it to a warning if you know what you are doing.

**Library naming**
Installed libraries are prefixed — `DsqtCore.lib`, `DsqtBridgeplugin.lib`,
`DsqtSpout.lib` — because vcpkg's prefix is shared with every other port and a
bare `Core.lib` would be a collision waiting to happen. The CMake target names
and the `Dsqt::` aliases are unchanged, so consumer code is unaffected.

Backing libraries are renamed with `OUTPUT_NAME`. QML plugins are **not**: Qt
writes the plugin target's name into the generated `qmldir` and
`qt6_import_qml_plugins()` resolves it verbatim, so the docs explicitly forbid
`OUTPUT_NAME` there. Plugins are renamed via `PLUGIN_TARGET` in
`qt_add_qml_module()` instead, which keeps the qmldir consistent. If you add a
module, follow the same split.

One exception: `bin/TouchEngine.dll` keeps its name, since the import library
records the DLL's real filename and renaming it would break loading.

**"The following files are already installed ... and are in conflict"**
Two ports are trying to own the same path. DsQt normally bundles
`SpoutDX_static.lib` into its prefix so a standalone install is self-contained,
but under vcpkg `spout2` is a declared dependency that installs it into the
shared prefix already. The port passes `-DDSQT_BUNDLE_DEPENDENCIES=OFF` to
suppress the copy. If you add another bundled third-party library, guard it the
same way.

**"There should be no installed empty directories"**
The per-module header installs use `install(DIRECTORY . FILES_MATCHING ...)`,
and CMake recreates the whole source tree under `include/` even where no header
matched. The portfile prunes empty directories after install rather than
carrying a hardcoded list. If you add a module and see this again, the prune
loop should already cover it -- check the directories are genuinely empty.

**`git archive` fails with exit code 128 during `vcpkg_from_git`**
Git LFS. `git archive` runs the working-tree conversion filters, and git-lfs
sets `filter.lfs.required=true` globally, so it aborts if the LFS objects were
never fetched. The portfile passes `LFS` to `vcpkg_from_git` to handle this;
check `git-lfs` is installed and on `PATH`. The real message is in
`<vcpkg>/buildtrees/dsqt/git-archive-err.log`.

**"expected the end of input parsing a package spec"**
The triplet name contains a character vcpkg rejects — almost always a dot from
a Qt version. Triplet names are lowercase alphanumerics and hyphens only:
`x64-windows-qt6-10-2`, not `x64-windows-qt6.10.2`.

**"Qt kit 6.10.2 (msvc2022_64) was requested but ... does not exist"**
The triplet names a kit that is not installed. Install it with the Qt
Maintenance Tool, or switch to a triplet matching a kit you have.

**DsQt rebuilds when you expected a cache hit**
Something in the hash changed. Common causes: a Visual Studio update (the
compiler binary is hashed), a new dsqt version, or an edited triplet. Run with
`--debug` to see the ABI hash inputs.

**Link errors about toml++ symbols**
`Dsqt::Core` is a static library, so the final link still has to resolve toml++
even though it is a private implementation detail. `DsqtConfig.cmake` pulls it
in via `find_dependency(tomlplusplus)`. Make sure your project resolves
`tomlplusplus` through the same vcpkg installation, not a second copy.

If the errors mention `__imp_` symbols, something built toml++ as a DLL. The
DsQt triplets force it static:

```cmake
if(PORT STREQUAL "tomlplusplus")
    set(VCPKG_LIBRARY_LINKAGE static)
endif()
```

Check that block is present in the triplet you are building with.
