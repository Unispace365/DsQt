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

## What you are changing

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

## 1. Check the application's starting point

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

## 2. Pin a published package

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

## 3. Choose the source in presets

The following is a complete **CMakeUserPresets.json** example for an application
that has the two helpers and manifest features above. It does not depend on
generated preset names. Merge it if you already have a user-presets file, and
adjust the paths. Start an x64 MSVC developer shell and set `VCPKG_ROOT` to your
bootstrapped checkout. Ninja must be available in that shell.

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "user-dsqt-base",
      "hidden": true,
      "generator": "Ninja Multi-Config",
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
        "DSQT_LOCAL_PACKAGE_ROOT": "C:/dev/DsQt/Library/build/vcpkg-package/qt6.12.0"
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

### Installed library to registry

Before: both mode switches off, with Qt and the installed DsQt prefix in
`CMAKE_PREFIX_PATH`. After: `DSQT_USE_REGISTRY_VCPKG=ON`, local mode off, and the
registry Qt variables set. Select `user-registry-6.12.0` above after pinning the
package in section 2. The helper sets Qt paths, the triplet, the manifest feature,
and `Dsqt_DIR` to the app's own `vcpkg_installed/<triplet>/share/dsqt`.

Remove source overlays from `VCPKG_OVERLAY_PORTS`, its environment variable, and
`vcpkg-configuration.json` before registry configuration. Registry mode rejects
these to ensure that a local recipe cannot replace the selected published port.
Do not carry an old installed `Dsqt_DIR` or `Qt6_DIR` into the new build directory.

### Registry to a local source package

Build a completed local package from your DsQt checkout first:

```bat
C:\dev\DsQt\Library\build_and_install.bat -vcpkg -qt 6.12.0 -modules Core,Bridge,Waffles
```

Select `user-local-6.12.0`. Its local mode switch enables `dsqt-local` and the
overlay from `DSQT_LOCAL_PACKAGE_ROOT`. The metadata chooses the exact Qt kit;
changing `CMAKE_PREFIX_PATH` cannot override that selection. Check that the
application's `builtin-baseline` matches `dsqt-local-package.json`. Align the
dependency baselines deliberately and rebuild as needed.

The [overlay takes precedence](https://learn.microsoft.com/en-us/vcpkg/concepts/overlay-ports)
over the registry recipe, so the registry override
can remain if you regularly switch between these separate presets. Its recorded
published version does not describe the local build. Inspect the local metadata
and installed package status to identify the snapshot actually used. If retiring
registry support entirely, remove its unused DsQt override and feature.

After source edits, repeat the package build and reconfigure the app. The package
root and default binary cache are local to your machine. Keep the package folder
in place; copying only its `installed/` folder is not a portable source package.
See the [local package guide](../vcpkg.md#build-a-local-package-and-use-it-in-a-new-application).

### Registry or local package to an installed library

Build/install DsQt with your exact Qt kit, or extract a compatible library ZIP.
Select `user-installed-6.12.0`, with both package switches off and paths pointing
to that installation. Do not enable `dsqt-registry` or `dsqt-local` through
`VCPKG_MANIFEST_FEATURES` or manifest default features in this mode. vcpkg still
provides the app's other dependencies.

The ordinary installation's config is `lib/cmake/Dsqt/DsqtConfig.cmake`; the
vcpkg layout uses `share/dsqt/DsqtConfig.cmake`. Preserve the template's deployment
helpers, which handle both layouts. Match the installed library's Qt version,
architecture, compiler compatibility, and Debug/Release binaries.

## 4. Change Qt or modules

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

For **modules**, keep `dsqt-project.json` and the dependency feature arrays in
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

## 5. Configure, build, and verify the result

Use a separate build directory when switching sources, Qt kits, compilers,
architectures, or generators. For a version-only update within the same mode and
kit, reconfigure so vcpkg reevaluates the manifest. If necessary, `cmake --fresh`
clears CMake's cache and `CMakeFiles`; it does not clear all installed dependencies
or deployed output. A new directory is the clearest diagnostic for stale state.
See [CMake's command-line reference](https://cmake.org/cmake/help/latest/manual/cmake.1.html#cmdoption-cmake-fresh).

Example for the complete registry preset above, in a matching x64 MSVC shell:

```powershell
$env:VCPKG_ROOT = 'C:/vcpkg'
cmake --preset user-registry-6.12.0
cmake --build --preset user-registry-release
cmake --install build/user-registry-6.12.0 --config Release
```

Inspect the actual result, rather than relying only on the UI label:

```powershell
$buildDir = 'build/user-registry-6.12.0'
Select-String -Path "$buildDir/CMakeCache.txt" -Pattern '^(Qt6_DIR|Dsqt_DIR|DSQT_USE_.*|DSQT_REGISTRY_QT_.*|VCPKG_TARGET_TRIPLET|VCPKG_MANIFEST_FEATURES):'
Select-String -Path "$buildDir/vcpkg_installed/vcpkg/status" -Pattern '^Package: dsqt$' -Context 0,12
```

Confirm the expected Qt config path, DsQt layout, active mode, triplet, and
installed DsQt version/features. In installed-library mode, check the selected
installation's `dsqt-modules.json` beside `DsqtConfig.cmake` instead of expecting
DsQt in the app's vcpkg status file. The configure log also reports the selected
package mode and Qt kit for registry/local builds.

Run the installed application from its deployed `bin` directory. Use the matching
Qt/CRT/plugin environment or the project's configured launcher, then verify its
selected modules and normal application workflows. For rollback, restore the
previous manifest, registry pin, and shared configuration together and configure
the corresponding build directory again.
