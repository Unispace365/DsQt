# Switching DsQt packages, versions, and Qt kits

For a new application, use **ProjectCloner** and its **Published vcpkg package**
source. For an existing application, change its configuration in place. Generate
a separate reference project if you need current scaffolding; merge the relevant
changes into your application rather than generating over it.

This guide covers Windows x64 with the MSVC Qt kit. The published Waffles package
`0.3.0-dev.224.af-waffle-glass` requires Qt **6.12.0** or newer compatible SDK,
including Canvas Painter. Disabling Waffles does not lower the current Core
module's Qt requirement. Check the selected package's metadata and install the
Qt components required by the modules you use.

## Choose your setup path

Complete [Before you start](#before-you-start), then follow the one path you need:

| I want to... | Follow this path |
| --- | --- |
| Use a published release, or upgrade/downgrade a published version | [Use a published DsQt package](#use-a-published-dsqt-package) |
| Build DsQt from my checkout and use the resulting local package | [Use a local DsQt source package](#use-a-local-dsqt-source-package) |
| Use a DsQt installation or an extracted library ZIP | [Use an installed DsQt library](#use-an-installed-dsqt-library) |

Each path uses the [shared preset example](#shared-preset-example-for-all-three-paths).
If your application already has the matching preset, use its actual name.
Finish with [Build and verify your selected setup](#build-and-verify-your-selected-setup).
You do not need to complete the other setup paths. After the shared file changes,
choose **either Option A — Qt Creator or Option B — Command line** to configure,
build, and install the application. Do not perform both application workflows.
The local DsQt package-build command is a shared prerequisite for local mode.

For an existing setup, see the optional tasks [Change the Qt kit](#change-the-qt-kit)
and [Add or remove modules](#add-or-remove-modules).

## Before you start

### Understand which files control the setup

| Change | Files/settings to update |
| --- | --- |
| Published DsQt version | Exact `dsqt` override in `vcpkg.json`, registry commit in `vcpkg-configuration.json`, and the selection record in `dsqt-project.json`. |
| Qt version or installation root | Registry preset's `DSQT_REGISTRY_QT_VERSION` / `DSQT_REGISTRY_QT_ROOT`. Local and installed binaries require their exact build kit. |
| Registry, local package, or installed library | Configure preset's source switches and paths; ensure the corresponding helper and manifest feature exist. Use a separate build directory. |
| Modules, such as Waffles or Qsys | `dsqt-project.json` modules, matching DsQt dependency features in `vcpkg.json`, and application code/resources for the added or removed module. |
| Debug versus Release | Build preset's `configuration` and install command's `--config`; keep the same multi-config configure preset. |

`CMakeLists.txt` describes how the application builds and calls
`find_package(Dsqt ...)`. **Once the current package helpers are integrated,
ordinary version or source changes do not require rewriting CMakeLists.txt.**

The files have different responsibilities:

- `CMakePresets.json`: shared toolchain, source mode, Qt selection, and build directories.
- `CMakeUserPresets.json`: local paths and additional presets; keep it out of Git.
- `vcpkg.json`: dependencies, requested DsQt features, and an exact DsQt override.
  Its top-level `version` is your **application's** version. `builtin-baseline`
  pins the Microsoft dependency catalog, separately from the DsQt registry.
- `vcpkg-configuration.json`: which registry supplies DsQt and the Git commit
  at which its version history is read.
- `dsqt-project.json`: ProjectCloner's selection record. The current application
  CMake reads `modules` and `minimumQt` from it. Editing its `source`, `path`, or
  `version` alone does **not** switch the CMake build or pin a dependency.

See [vcpkg version selection](https://learn.microsoft.com/en-us/vcpkg/users/versioning)
for the distinction between baselines and overrides.

<a id="1-check-the-applications-starting-point"></a>

### Check your application and prepare older projects

From the application directory:

```powershell
cmake --list-presets
cmake --list-presets=build
Get-Content dsqt-project.json
Get-Content vcpkg.json
Get-Content vcpkg-configuration.json
```

ProjectCloner generates presets for the **chosen source**, not all three modes.
For Qt 6.12.0, typical Ninja configure names are `ninja-vcpkg-6.12.0` (registry),
`ninja-vcpkg-local` (local), and `ninja-6.12.0` (installed). Use the names actually
listed in your project. The multi-config build presets add `-debug` or `-release`.

If an older application lacks the current helpers, copy
[`DsqtRegistryPackage.cmake`](../../Examples/ClonerSource/cmake/DsqtRegistryPackage.cmake)
and [`DsqtLocalPackage.cmake`](../../Examples/ClonerSource/cmake/DsqtLocalPackage.cmake)
into its `cmake/` directory. Include them **before** the existing `project()` call:

```cmake
# Insert before the application's existing project(...) call.
include(cmake/DsqtRegistryPackage.cmake)
include(cmake/DsqtLocalPackage.cmake)
```

vcpkg runs during `project()`, so including these afterward is too late. Preserve
the rest of your application setup. After Qt setup, request your actual modules:

```cmake
# Example for an application using Core, Bridge, and Waffles.
find_package(Dsqt CONFIG REQUIRED COMPONENTS Core Bridge Waffles)
target_link_libraries(myapp PRIVATE Dsqt::Core Dsqt::Bridge Dsqt::Waffles)
```

Substitute your existing target for `myapp`; do not add a second executable.
Keep the template's static QML plugin imports, `IMPORT_PATH
"${DSQT_QML_IMPORT_PATH}"` on `qt_add_qml_module`, and layout-aware deployment
helpers. Older `add_subdirectory` applications should first follow the
[migration guide](migration_subdirectory_to_library.md).

<a id="3-choose-the-source-in-presets"></a>

## Shared preset example for all three paths

This is a reference for all three paths, not a requirement to enable all three.
Use `user-dsqt-base`, the configure preset for your chosen path, and its matching
Debug/Release build presets. The paths below explain the settings to use.

The following is a complete **CMakeUserPresets.json** example for an application
that has the two helpers described above. Registry and local mode also need
the corresponding manifest feature from the [manifest example](#pin-the-published-version).
It does not depend on generated preset names. Merge it if you already have a
user-presets file, and adjust the paths. Set `VCPKG_ROOT` to your bootstrapped
checkout in the preset's environment or in the Qt Creator process environment
for Option A. For Option B, use an x64 MSVC developer shell with Ninja available.
The same presets support either option.

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "user-dsqt-base",
      "hidden": true,
      "generator": "Ninja Multi-Config",
      "architecture": { "value": "x64", "strategy": "external" },
      "toolset": { "value": "host=x64", "strategy": "external" },
      "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
      "cacheVariables": {
        "CMAKE_CONFIGURATION_TYPES": "Debug;Release",
        "CMAKE_C_COMPILER": "cl.exe",
        "CMAKE_CXX_COMPILER": "cl.exe",
        "CMAKE_CXX_FLAGS_INIT": "/EHsc"
      }
    },
    {
      "name": "user-registry-6.12.0",
      "inherits": "user-dsqt-base",
      "binaryDir": "${sourceDir}/build/user-registry-6.12.0",
      "cacheVariables": {
        "DSQT_USE_REGISTRY_VCPKG": "ON",
        "DSQT_USE_LOCAL_VCPKG": "OFF",
        "DSQT_REGISTRY_QT_VERSION": "6.12.0",
        "DSQT_REGISTRY_QT_ROOT": "C:/Qt"
      }
    },
    {
      "name": "user-local-6.12.0",
      "inherits": "user-dsqt-base",
      "binaryDir": "${sourceDir}/build/user-local-6.12.0",
      "cacheVariables": {
        "DSQT_USE_REGISTRY_VCPKG": "OFF",
        "DSQT_USE_LOCAL_VCPKG": "ON",
        "DSQT_LOCAL_PACKAGE_ROOT": "$env{LOCALAPPDATA}/DsQt/vcpkg"
      }
    },
    {
      "name": "user-installed-6.12.0",
      "inherits": "user-dsqt-base",
      "binaryDir": "${sourceDir}/build/user-installed-6.12.0",
      "cacheVariables": {
        "DSQT_USE_REGISTRY_VCPKG": "OFF",
        "DSQT_USE_LOCAL_VCPKG": "OFF",
        "CMAKE_PREFIX_PATH": "C:/Qt/6.12.0/msvc2022_64;$env{USERPROFILE}/Documents/DsQt",
        "Dsqt_DIR": "$env{USERPROFILE}/Documents/DsQt/lib/cmake/Dsqt",
        "VCPKG_TARGET_TRIPLET": "x64-windows",
        "VCPKG_INSTALLED_DIR": "${sourceDir}/build/user-installed-6.12.0/vcpkg_installed"
      }
    }
  ],
  "buildPresets": [
    { "name": "user-registry-release", "configurePreset": "user-registry-6.12.0", "configuration": "Release" },
    { "name": "user-local-release", "configurePreset": "user-local-6.12.0", "configuration": "Release" },
    { "name": "user-installed-release", "configurePreset": "user-installed-6.12.0", "configuration": "Release" },
    { "name": "user-registry-debug", "configurePreset": "user-registry-6.12.0", "configuration": "Debug" },
    { "name": "user-local-debug", "configurePreset": "user-local-6.12.0", "configuration": "Debug" },
    { "name": "user-installed-debug", "configurePreset": "user-installed-6.12.0", "configuration": "Debug" }
  ]
}
```

For shared team support, put the portable presets in `CMakePresets.json` and keep
machine-specific overrides in the user file. Names must be unique across both.
See [CMake user presets](cmake_user_presets.md) for inheritance examples.

<a id="2-pin-a-published-package"></a>

## Use a published DsQt package

Follow this path to select a published version and configure your application to use it.

### Find available published versions

In **ProjectCloner**, select **Published vcpkg package**, then **Refresh**.
Select a version to review its modules and Qt requirements. Generate a separate
reference project if you need the matching pins and selection record; do not
generate over your existing application.

As an alternative to ProjectCloner discovery, inspect the private registry in PowerShell. Choose an unused
checkout directory and ensure Git can authenticate to the repository:

```powershell
git clone https://github.com/Unispace365/vcpkg-registry C:/dev/dsqt-registry
Set-Location C:/dev/dsqt-registry
$catalog = Get-Content versions/d-/dsqt.json -Raw | ConvertFrom-Json
$catalog.versions | Select-Object version, 'port-version'
git rev-parse HEAD
```

Record the selected version, its `port-version`, and the full registry commit
SHA printed by the final command. Use values from this same registry snapshot
for the override and registry pin below. For an existing clean registry checkout,
update it with `git pull --ff-only` before listing versions and recording its SHA.
Return to your application directory before configuring the application.

The catalog establishes which versions are available, not whether every version
is compatible with your application. Check the selected package's metadata and
Qt/module requirements. A GitHub release tag alone does not establish registry
availability. Published sources also require Git LFS and Git access to the
private DsQt repository. vcpkg builds the source or restores cached binaries;
you do not need to build/install DsQt separately for this path.

### Pin the published version

ProjectCloner writes these pins when you choose a version. For an existing app,
merge the following **fragments** into its files, preserving unrelated
dependencies, features, registries, and overrides.

This concrete example selects the published
[`0.3.0-dev.224.af-waffle-glass` release](https://github.com/Unispace365/DsQt/releases/tag/library/v0.3.0-dev.224.af-waffle-glass).
Its registry version entry is present at commit
[`40c745a4552ece07cea659933844c858a9d88d59`](https://github.com/Unispace365/vcpkg-registry/blob/40c745a4552ece07cea659933844c858a9d88d59/versions/d-/dsqt.json).
These are reproducible example pins, not a claim that this will always be the
latest release.

`vcpkg.json` fragment, using Core, Bridge, and Waffles:

```json
{
  "features": {
    "dsqt-registry": {
      "description": "Use DsQt from the published registry",
      "dependencies": [
        { "name": "dsqt", "default-features": false, "features": ["bridge", "waffles"] }
      ]
    },
    "dsqt-local": {
      "description": "Use a local DsQt source package",
      "dependencies": [
        { "name": "dsqt", "default-features": false, "features": ["bridge", "waffles"] }
      ]
    }
  },
  "overrides": [
    { "name": "dsqt", "version": "0.3.0-dev.224.af-waffle-glass#0" }
  ]
}
```

Core is implicit; it has no vcpkg feature. `#0` is the port revision. It is
independent of the build number `224` and does not need incrementing by consumers.
Keep DsQt in the mode features, rather than also as an unconditional dependency,
if the same app must support installed-library mode.
If the app already declares DsQt as an unconditional dependency, move that
declaration into the corresponding mode feature instead of duplicating it.

`vcpkg-configuration.json` fragment:

```json
{
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/Unispace365/vcpkg-registry",
      "baseline": "40c745a4552ece07cea659933844c858a9d88d59",
      "packages": ["dsqt"]
    }
  ]
}
```

To upgrade or downgrade, select a version/revision that exists in the registry,
then update both pins together. The registry commit must contain the desired
version; a newer commit may also contain older versions. Update the selection
record's `source`, `version`, `portVersion`, `repository`, `baseline`, and package
metadata to match. A temporary ProjectCloner project using that package is a
convenient source of the exact record; retain your application's intended modules.

At the example registry commit, the default DsQt baseline remains `0.2.0.4`.
Development publication intentionally leaves it unchanged. Updating only the
registry commit therefore does not select the Waffles development build. Do not
invent a minimum such as `0.3.0-0`; requested minimum versions must exist.

Neither changing `Library/VERSION.txt` nor changing the app's top-level
`vcpkg.json` version selects a published dependency.

### Select the registry preset

Set `DSQT_USE_REGISTRY_VCPKG=ON`, local mode off, and the registry Qt variables.
Select `user-registry-6.12.0` from the [shared preset example](#shared-preset-example-for-all-three-paths)
after pinning the package above. The helper sets Qt paths, the triplet, the manifest feature,
and `Dsqt_DIR` to the app's own `vcpkg_installed/<triplet>/share/dsqt`.

Remove source overlays from `VCPKG_OVERLAY_PORTS`, its environment variable, and
`vcpkg-configuration.json` before registry configuration. Registry mode rejects
these to ensure that a local recipe cannot replace the selected published port.
Do not carry an old installed `Dsqt_DIR` or `Qt6_DIR` into the new build directory.

Continue with [Build and verify your selected setup](#build-and-verify-your-selected-setup).
Choose **either Option A — Qt Creator or Option B — Command line**, using
`user-registry-6.12.0` as the configure preset.

## Use a local DsQt source package

Use this path for a package built from your own DsQt checkout. Your application
needs the local helper from [Before you start](#before-you-start), the
`dsqt-local` feature shown in the [manifest example](#pin-the-published-version),
and `user-local-6.12.0` from the [shared preset example](#shared-preset-example-for-all-three-paths).
The published-version pin and registry preset are only needed if you also use
the published-package path.

This workflow creates a local package store with an overlay port. You do not
need to initialize a separate Git registry or add a GitHub DsQt registry entry
for a local-only setup.

Shared prerequisite for either application workflow: build and publish a completed
local package from your DsQt checkout first:

```bat
set "VCPKG_ROOT=C:\vcpkg"
C:\dev\DsQt\Library\build_and_install.bat -vcpkg -qt 6.12.0 -modules "Core,Bridge,Waffles"
```

Replace the paths, Qt version, and modules for your application. Successful
builds publish to `%LOCALAPPDATA%\DsQt\vcpkg` and automatically mark the completed
package as that store's default. The builder prepares MSVC x64 and builds Debug
and Release. A full Qt kit path can be passed to `-qt` for a custom installation.
Select `user-local-6.12.0` to use the default package. The helper reads
`dsqt-local-default.json` automatically to locate it; **do not edit or manage
that file manually**. With no explicit package path, the local helper discovers
the same store; an explicit CMake/preset path overrides the environment default.
Its local mode switch enables `dsqt-local` and the selected overlay.
The metadata chooses the exact Qt kit;
changing `CMAKE_PREFIX_PATH` cannot override that selection. The preset name
does not select a different Qt kit. To inspect the default package in PowerShell:

```powershell
$store = "$env:LOCALAPPDATA/DsQt/vcpkg"
$selection = Get-Content "$store/dsqt-local-default.json" -Raw | ConvertFrom-Json
$metadata = Get-Content "$($selection.packageRoot)/dsqt-local-package.json" -Raw |
    ConvertFrom-Json
$metadata | Select-Object packageVersion, qtVersion, qtKit, builtinBaseline
```

For a custom store, change `$store`. For an exact snapshot selection, read
`dsqt-local-package.json` directly from that snapshot directory. Compare its
`builtinBaseline` with the application's `builtin-baseline` in `vcpkg.json`.
Align dependency baselines deliberately and rebuild as needed; the helper
rejects a mismatch before installing dependencies.

The [overlay takes precedence](https://learn.microsoft.com/en-us/vcpkg/concepts/overlay-ports)
over the registry recipe, so the registry override
can remain if you regularly switch between these separate presets. Its recorded
published version does not describe the local build. Inspect the local metadata
and installed package status to identify the snapshot actually used. If retiring
registry support entirely, remove its unused DsQt override and feature.

After source edits, repeat the package build above. Then choose the update step
for your application workflow:

- **Option A — Qt Creator:** use **Re-configure with Initial Variables**, then
  rebuild and run the `install` target as described in Option A. Keep the initial
  preset settings. You do not need to delete `.qtcreator` for every source update.
- **Option B — Command line:** run the following instead, from a matching x64
  MSVC developer shell in the application directory:

  ```powershell
  cmake --fresh --preset user-local-6.12.0
  cmake --build --preset user-local-release
  cmake --install build/user-local-6.12.0 --config Release
  ```

These names match the complete preset example above; substitute your actual
names. ProjectCloner's default local selection records the store; selecting an
exact snapshot directory instead pins that snapshot. To switch an older app to
the store, set `DSQT_LOCAL_PACKAGE_ROOT` to `$env{LOCALAPPDATA}/DsQt/vcpkg` as shown
in the JSON example and configure fresh.
Published snapshots are retained, and failed builds do not replace the default.
The package and binary cache are local to your machine.
See the [local package guide](../vcpkg.md#build-a-local-package-and-use-it-in-a-new-application).

Continue with [Build and verify your selected setup](#build-and-verify-your-selected-setup).
Choose **either Option A — Qt Creator or Option B — Command line**, using
`user-local-6.12.0` as the configure preset.

## Use an installed DsQt library

Use this path for an existing DsQt installation or library ZIP. Use
`user-installed-6.12.0` from the [shared preset example](#shared-preset-example-for-all-three-paths).
The published-version pins and local-package build are not required for this path.

Build/install DsQt with your exact Qt kit, or extract a compatible library ZIP.
Select `user-installed-6.12.0`, with both package switches off and paths pointing
to that installation. Do not enable `dsqt-registry` or `dsqt-local` through
`VCPKG_MANIFEST_FEATURES` or manifest default features in this mode. vcpkg still
provides the app's other dependencies.

The ordinary installation's config is `lib/cmake/Dsqt/DsqtConfig.cmake`; the
vcpkg layout uses `share/dsqt/DsqtConfig.cmake`. Preserve the template's deployment
helpers, which handle both layouts. Match the installed library's Qt version,
architecture, compiler compatibility, and Debug/Release binaries.

Continue with [Build and verify your selected setup](#build-and-verify-your-selected-setup).
Choose **either Option A — Qt Creator or Option B — Command line**, using
`user-installed-6.12.0` as the configure preset.

<a id="4-change-qt-or-modules"></a>

## Change the Qt kit

For a **registry build**, duplicate the registry preset and change
`DSQT_REGISTRY_QT_VERSION` to the exact installed version. Change its name and
`binaryDir` too. `DSQT_REGISTRY_QT_ROOT` is the parent root such as `D:/Qt`, not
the kit directory; the helper expects `<root>/<version>/msvc2022_64`. Update the
build preset's `configurePreset` to the new name. The helper creates a matching
triplet, and vcpkg builds or restores the matching package.

For a **local package**, rebuild the package with `-qt` for the new kit and point
a new preset at its resulting package root. For an **installed library**, build
or obtain a matching installation and update both the Qt and DsQt paths in a new
preset. An installed library is not rebuilt by changing the consumer's Qt path.

## Add or remove modules

Keep `dsqt-project.json` and the dependency feature arrays in
both `dsqt-registry` and `dsqt-local` aligned. For example, adding Waffles to
Core-only changes modules to `["Core", "Waffles"]` and dependency features to
`["waffles"]`. Keep `default-features` false to avoid enabling every module.
Retain other modules your app uses. Check the selected metadata's requirements
and adjust `minimumQt` if needed; lowering it cannot bypass the package's minimum.
An installed library must already contain the modules you request.

ProjectCloner omits unselected module files. Adding a module later also requires
its startup code, plugin registration, QML, settings, and deployment resources.
Compare a separately generated project with that module selected. For Waffles,
follow the [existing-app integration path](../waffles-implementation-guide.md#existing-dsqt-application-integrate-in-place).

<a id="5-configure-build-and-verify-the-result"></a>

## Build and verify your selected setup

Use a separate build directory when switching sources, Qt kits, compilers,
architectures, or generators. For a version-only update within the same mode and
kit, reconfigure so vcpkg reevaluates the manifest. If necessary, `cmake --fresh`
clears CMake's cache and `CMakeFiles`; it does not clear all installed dependencies
or deployed output. A new directory is the clearest diagnostic for stale state.
See [CMake's command-line reference](https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-fresh).

Choose the names and build directory for your setup from this table. These
match the [shared preset example](#shared-preset-example-for-all-three-paths);
substitute your actual names if you use generated or existing presets.

| Setup path | Configure preset | Release build preset | Build directory |
| --- | --- | --- | --- |
| Published package | `user-registry-6.12.0` | `user-registry-release` | `build/user-registry-6.12.0` |
| Local source package | `user-local-6.12.0` | `user-local-release` | `build/user-local-6.12.0` |
| Installed library | `user-installed-6.12.0` | `user-installed-release` | `build/user-installed-6.12.0` |

<a id="import-the-presets-into-qt-creator"></a>

### Option A — Qt Creator (choose this or Option B)

Complete your chosen source path's helper, manifest, and preset changes first.
For local mode, build and publish the local package before opening the app.
Use the [shared preset example](#shared-preset-example-for-all-three-paths) or
equivalent presets in `CMakePresets.json`. Do not inherit installed-library
paths in a package preset. Review existing `CMakeUserPresets.json` overrides
for stale paths too; resetting Qt Creator does not remove that file.

Use this option to configure, build, and install inside Qt Creator. Skip Option B.

For a clean import when switching an existing application's source mode:

1. Close Qt Creator. Save any custom run/debug settings you want to recreate.
2. Delete the **application's** `.qtcreator` directory and its old generated
   build directory after checking that the build directory contains only
   generated output. Older projects may also store `CMakeLists.txt.user` beside
   `CMakeLists.txt`; remove that saved settings file if present. Keep the local
   package store and the DsQt library's package-build directory.
3. Set `VCPKG_ROOT` in the configure preset's `environment` object, for example
   `"environment": { "VCPKG_ROOT": "C:/vcpkg" }`. Merge it with any existing
   environment entries. Keep machine-specific paths in `CMakeUserPresets.json`.
   This lets you launch Qt Creator normally without a terminal setup step.
4. Open the application's `CMakeLists.txt` and select the configure preset for
   your chosen path: `user-registry-6.12.0`, `user-local-6.12.0`, or
   `user-installed-6.12.0`. Substitute your actual preset names. Verify the
   imported kit has x64 MSVC C and C++ compilers and the intended Qt kit.
5. Configure, select Debug or Release, and build. Under **Projects > Build
   Settings > Build Steps**, select the CMake `install` target and build it for
   the same configuration. This runs the project's install/deployment rules;
   no command-line install step is required. Restore the normal application/all
   target afterward if you changed the existing build step's target selection.
6. In CMake output and **Current Configuration**, confirm the selected Qt kit,
   source mode, and `Dsqt_DIR`. Check the installed version/features in the
   build directory's `vcpkg_installed/vcpkg/status` using your editor. For an
   installed library, inspect `dsqt-modules.json` beside `DsqtConfig.cmake`.
   Then follow [Verify the selected package and application](#verify-the-selected-package-and-application).

The shared Ninja preset uses `external` for both `architecture` and `toolset`
so Qt Creator can initialize MSVC before CMake runs. CMake itself does not set
up MSVC from these fields; command-line builds still need the matching developer
shell. See [Qt Creator's MSVC preset example](https://doc.qt.io/qtcreator/creator-build-settings-cmake-presets.html#msvc-example).
Qt Creator imports presets when opening a project without saved project settings.
For later preset edits, **Build > Reload CMake Presets** is also available.
See [preset import](https://doc.qt.io/qtcreator/creator-build-settings-cmake-presets.html)
and [reconfiguring with initial variables](https://doc.qt.io/qtcreator/creator-build-settings-cmake.html#re-configuring-with-initial-variables).

See [Qt Creator CMake build steps](https://doc.qt.io/qtcreator/creator-build-settings-cmake.html#cmake-build-steps) for target selection.

### Option B — Command line (choose this or Option A)

Use this option instead of Qt Creator to configure, build, and install. No IDE
reset/import is needed. Run from the application directory in a matching x64
MSVC developer shell.

Example for the published-package path, in a matching x64 MSVC shell.
For another path, replace both preset names and the install build directory
using the table above:

```powershell
$env:VCPKG_ROOT = 'C:/vcpkg'
cmake --preset user-registry-6.12.0
cmake --build --preset user-registry-release
cmake --install build/user-registry-6.12.0 --config Release
```

For this option, inspect the selected package with these commands. Set
`$buildDir` to your selected path's build directory from the table above:

```powershell
$buildDir = 'build/user-registry-6.12.0'
Select-String -Path "$buildDir/CMakeCache.txt" -Pattern '^(Qt6_DIR|Dsqt_DIR|DSQT_USE_.*|DSQT_REGISTRY_QT_.*|VCPKG_TARGET_TRIPLET|VCPKG_MANIFEST_FEATURES):'
Select-String -Path "$buildDir/vcpkg_installed/vcpkg/status" -Pattern '^Package: dsqt$' -Context 0,12
```

### Verify the selected package and application

For either option, confirm the expected Qt config path, DsQt layout, active mode, triplet, and
installed DsQt version/features. In installed-library mode, check the selected
installation's `dsqt-modules.json` beside `DsqtConfig.cmake` instead of expecting
DsQt in the app's vcpkg status file. The configure log also reports the selected
package mode and Qt kit for registry/local builds.

Run the installed application from its deployed `bin` directory. Use the matching
Qt/CRT/plugin environment or the project's configured launcher, then verify its
selected modules and normal application workflows. For rollback, restore the
previous manifest, registry pin, and shared configuration together and configure
the corresponding build directory again.
