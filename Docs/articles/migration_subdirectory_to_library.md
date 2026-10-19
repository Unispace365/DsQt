# Migrating from DsQt Subdirectory to DsQt Library

This guide moves older `add_subdirectory()` consumers to `find_package(Dsqt)`.
The recommended source for applications is a **published vcpkg package from the
registry**. An ordinary prebuilt installation remains an alternative.

For a new application, use [ProjectCloner](../../README.md#creating-a-new-project).
For an existing application already using `find_package`, go directly to
[Switching DsQt packages, versions, and Qt kits](switching_dsqt_packages.md).
That guide covers registry pins, source switches, and complete presets.

## Automated migration for the installed-library path

`Tools/migrate_to_library.py` automates much of the older installed-library
migration. It is not a registry-version selector: review its output, then add
the current package helpers and pins from the switching guide for registry use.
Preview with `--dry-run` and preserve application-specific CMake, startup code,
and dependencies when applying the changes.

### Usage

```bash
python Tools/migrate_to_library.py <project_directory> [options]
```

### Options

| Option | Description |
|---|---|
| `<project_directory>` | Path to the project to migrate (required) |
| `--dsqt-root <path>` | Path to DsQt repo root. Default: derived from script location |
| `--dry-run` | Show what would change without modifying any files |
| `--no-backup` | Skip the git backup prompt |
| `--reset` | Reset the project to the pre-migration backup branch |

### Examples

```bash
# Preview changes without modifying files
python Tools/migrate_to_library.py C:\dev\MyProject --dry-run

# Run the migration
python Tools/migrate_to_library.py C:\dev\MyProject

# Run without the git backup prompt
python Tools/migrate_to_library.py C:\dev\MyProject --no-backup

# Undo a migration (restores the pre-migration-backup branch)
python Tools/migrate_to_library.py C:\dev\MyProject --reset
```

### What the script does

1. Parses your existing `CMakeLists.txt` to extract project metadata, Qt components, QML files, link targets, and local subdirectories
2. Optionally creates a git backup branch (`pre-migration-backup`)
3. Generates a new `CMakeLists.txt` using `find_package(Dsqt)` instead of `add_subdirectory()`
4. Generates or patches `CMakePresets.json` with detected Qt installations and the DsQt install path (use `CMakeUserPresets.json` for local overrides — see Step 6)
5. Creates `vcpkg.json` and `VERSION.txt` if missing
6. Patches `main.cpp` with `Q_IMPORT_QML_PLUGIN` macros
7. Rewrites QML imports (`import Dsqt` → `import Dsqt.Core`, etc.)
8. Copies missing scaffolding files from `Examples/ClonerSource/`

After reviewing the migration, configure and build in a new directory. Keep the
old build available for comparison until the migrated application is validated.

---

## Manual Migration Steps

The following sections describe each migration step in detail. If you used the automated script above, these serve as a reference for understanding and troubleshooting the changes.

## Prerequisites

- An x64 MSVC compiler compatible with the selected Qt kit
- Qt 6.12.0 or newer compatible SDK for current Core/Waffles, including Canvas Painter
- CMake 3.29 or newer, Ninja, Git, and Git LFS
- vcpkg installed and `VCPKG_ROOT` environment variable set
- The DsQt repository cloned locally

## Step 1: Choose the DsQt package source

**Recommended: registry.** Choose a published version with supported module
metadata in a separately generated ProjectCloner reference app. Merge its exact
DsQt override and registry commit into your application, following the
[switching guide](switching_dsqt_packages.md#2-pin-a-published-package). You do
not build/install a standalone DsQt library first; vcpkg builds it for the app.

**Alternative: installed library.** Run
`Library/build_and_install.bat -qt 6.12.0` from the DsQt checkout, substituting your
installed kit. This builds Debug and Release and installs to
`%USERPROFILE%/Documents/DsQt` by default. Match that exact kit in the consumer,
and rebuild/reinstall when changing the library. For a source snapshot consumed
through vcpkg, see [local packages](../vcpkg.md#build-a-local-package-and-use-it-in-a-new-application).

## Step 2: Copy Project Scaffolding from ClonerSource

Several files in the `Examples/ClonerSource` directory serve as the reference template for a library-based project. Merge the relevant pieces into your project, preserving existing application logic:

### Files to Copy

| Source (from `Examples/ClonerSource/`) | Destination (your project root) | Notes |
|---|---|---|
| `CMakeLists.txt` | `CMakeLists.txt` | **Merge.** Adopt package discovery and deployment while keeping your targets and resources; see Step 3. |
| `CMakePresets.json` | `CMakePresets.json` | **Merge.** Choose registry, local-package, or installed-library presets. Use user presets for machine-specific paths (Step 6). |
| `vcpkg.json` | `vcpkg.json` | **Merge.** Preserve app dependencies and add the selected DsQt mode feature and version override. |
| `main.cpp` | `main.cpp` | **Use as reference.** Merge static plugin imports and initialization without replacing existing services. |
| `cmake/DsqtRegistryPackage.cmake`, `cmake/DsqtLocalPackage.cmake` | Same paths | Copy for package modes; include before `project()`. |
| `vcpkg-configuration.json`, `dsqt-project.json` | Same paths | Merge the selected registry pin and module record from a generated reference project. |
| `cmake/app.rc.in` | `cmake/app.rc.in` | Windows resource file template. Copy if you don't already have one. |
| `resource1.h` | `resource1.h` | Windows resource header. Copy if you don't already have one. |
| `VERSION.txt` | `VERSION.txt` | Version source of truth. Create with your version (e.g. `1.0.0`). |
| `.gitignore` | `.gitignore` | **Merge** with your existing `.gitignore`. |
| `project-ci.yml` | `project-ci.yml` | CI configuration template. Copy and update the `name` and `qt_modules` fields. |
| `settings/` | `settings/` | Copy the full directory if starting fresh, or merge individual `.toml` files. |

> Generate a separate **ProjectCloner** reference app with the desired package
> and modules to compare its scaffolding. Integrate into your existing application
> instead of overwriting it with the raw template.

## Step 3: Update CMakeLists.txt

This is the core of the migration. Below are the specific changes needed.

### Remove: Subdirectory includes and environment variable references

Delete these lines (or similar):

```cmake
# REMOVE these lines
list(APPEND CMAKE_MODULE_PATH "$ENV{DS_QT_PLATFORM_100}/Dsqt/cmake/")
add_subdirectory("$ENV{DS_QT_PLATFORM_100}/Dsqt" "${CMAKE_BINARY_DIR}/Dsqt")
```

### Add: find_package for DsQt

Replace the removed lines with:

```cmake
# Package helpers or the installed-library preset select the package location.
find_package(Dsqt REQUIRED COMPONENTS Core Bridge Waffles TouchEngine)
```

Only include the components your project actually uses. If you don't use TouchEngine, omit it.

### Update: target_link_libraries

Replace the old internal target names with the new namespaced targets.

**Before (subdirectory approach):**
```cmake
target_link_libraries(${DS_EXE_NAME}
    PRIVATE
        Qt6::Quick
        Dsqt
        Dsqtplugin
        Dsqtplugin_init
        Dsqt_resources_1
        Dsqt_resources_2
        Dsqt_resources_3
        glm::glm
        PkgConfig::tomlplusplus
        Qt6::Sql
        Qt6::GuiPrivate
        # ...
)
```

**After (package approach):**
```cmake
target_link_libraries(${DS_EXE_NAME}
    PRIVATE
        Dsqt::Core
        Dsqt::Bridge
        Dsqt::TouchEngine
        Dsqt::Waffles
        Qt6::Quick
)
```

Preserve additional dependencies used directly by your application. DsQt's
imported targets carry their own include paths and link dependencies. Remove
manual source-tree include paths; do not add `PkgConfig::tomlplusplus` just to
satisfy DsQt. If the app uses toml++ directly, find its CMake package and link
`tomlplusplus::tomlplusplus` as in the current ClonerSource template.

### Update: qt_add_qml_module

Add `IMPORT_PATH` and `DEPENDENCIES` for the Dsqt modules:

```cmake
qt_add_qml_module(${DS_EXE_NAME}
    URI ${DS_APP_NAME}
    VERSION 1.0
    QML_FILES
        Main.qml
        # ... your QML files
    IMPORT_PATH "${DSQT_QML_IMPORT_PATH}"
    DEPENDENCIES
        Dsqt.Core
        Dsqt.Bridge
        Dsqt.TouchEngine
        Dsqt.Waffles
        QtQuick
)
```

### Add: layout-aware runtime deployment

Use the selected package's helpers after creating the application target:

```cmake
dsqt_deploy_runtime_dlls(${DS_EXE_NAME})
```

After setting up the application's install directories, add:

```cmake
dsqt_install_runtime_dlls(${CMAKE_INSTALL_BINDIR})
```

These handle the requested modules' runtime DLLs, including TouchEngine, across
ordinary and vcpkg layouts. Do not hardcode `bin/$<CONFIG>/TouchEngine.dll` for
vcpkg: its Debug and Release directories differ from an ordinary installation.
Keep the application's Qt QML deployment script as well.

## Step 4: Update main.cpp

### Add: QML plugin imports

At the top of `main.cpp`, add static QML plugin imports for each Dsqt module you use:

```cpp
#include <QtQml/qqmlextensionplugin.h>

// Import statically linked Dsqt QML plugins (guarded by CMake compile definitions)
#ifdef DSQT_HAS_Core
Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)
#endif
#ifdef DSQT_HAS_Bridge
Q_IMPORT_QML_PLUGIN(Dsqt_BridgePlugin)
#endif
#ifdef DSQT_HAS_TouchEngine
Q_IMPORT_QML_PLUGIN(Dsqt_TouchEnginePlugin)
#endif
#ifdef DSQT_HAS_Waffles
Q_IMPORT_QML_PLUGIN(Dsqt_WafflesPlugin)
#endif
#ifdef DSQT_HAS_Spout
Q_IMPORT_QML_PLUGIN(Dsqt_SpoutPlugin)
#endif
```

The application sets these `DSQT_HAS_<Module>` compile definitions using the
component results from `find_package(Dsqt)`. Add this pattern after creating the
application target, with the module list matching your requested components:

```cmake
foreach(_comp Core Bridge Waffles TouchEngine Spout)
    if(Dsqt_${_comp}_FOUND)
        target_compile_definitions(${DS_EXE_NAME} PRIVATE DSQT_HAS_${_comp})
    endif()
endforeach()
```

Only requested modules get these imports. A missing component listed as
`REQUIRED` still fails configuration; the guard does not make a required module optional.

### Update: Module loading

If your old code loaded QML via a file path, update it to load from a module:

**Before:**
```cpp
engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
```

**After:**
```cpp
engine.loadFromModule("YourAppName", "Main");
```

Where `"YourAppName"` matches the `URI` in your `qt_add_qml_module` call.

## Step 5: Update QML Imports

The library approach uses modular QML import names. Update all `import` statements in your `.qml` files:

| Old Import | New Import |
|---|---|
| `import Dsqt` | `import Dsqt.Core` |
| `import WafflesUx` | `import Dsqt.Waffles` |
| `import TouchEngineQt` | `import Dsqt.TouchEngine` |

For example, a file that had:

```qml
import Dsqt
import TouchEngineQt
```

Should become:

```qml
import Dsqt.Core
import Dsqt.TouchEngine
```

Only the bare module names need updating. If a QML file uses components from multiple DsQt modules (e.g. both Core and Bridge), add separate imports for each:

```qml
import Dsqt.Core
import Dsqt.Bridge
```

> **Tip:** The migration script (`Tools/migrate_to_library.py`) handles this automatically.

## Step 6: Update CMake Presets

Use the complete [source-mode presets](switching_dsqt_packages.md#3-choose-the-source-in-presets)
for registry, local package, or installed library. They use separate build
directories and do not depend on historical template preset names.

For registry mode, set `DSQT_USE_REGISTRY_VCPKG=ON`, local mode off, and the
registry Qt version/root. The helper controls the package paths. For installed
mode, turn both package switches off and include the Qt kit and DsQt installation
in `CMAKE_PREFIX_PATH`, with `Dsqt_DIR` identifying that installation.

Keep portable, team-wide configuration in `CMakePresets.json`. Put local path
changes in `CMakeUserPresets.json`. Inherit from a configure preset, never a
`-debug` or `-release` build preset. See [CMake user presets](cmake_user_presets.md)
for examples. Review any presets regenerated by the migration script before use.

## Step 7: Remove source-tree dependencies

Remove the application's dependency on `DS_QT_PLATFORM_100` for finding DsQt
source. `find_package` uses the selected package configuration instead. Other
legacy projects or tools may still use that environment variable, so changing
this application does not require removing it from your machine globally.

## Step 8: Configure and validate in a new build directory

1. Inspect configure/build names with `cmake --list-presets` and `cmake --list-presets=build`.
2. Configure the chosen source-mode preset in its separate build directory.
3. Build the intended configuration and install with the matching `--config`.
4. Verify `Qt6_DIR`, `Dsqt_DIR`, and the installed package version using the
   [verification steps](switching_dsqt_packages.md#5-configure-build-and-verify-the-result).
5. Launch the deployed app with the matching Qt runtime and verify existing
   features plus any newly selected modules.

## Quick Reference: Old vs New Target Names

| Old (Subdirectory) | New (Library) |
|---|---|
| `Dsqt` | `Dsqt::Core` (or the umbrella `Dsqt::Dsqt`) |
| `Dsqtplugin` | `Dsqt::Coreplugin` |
| `Dsqt_resources_1`, `_2`, `_3` | Handled automatically by the imported targets |
| N/A (monolithic) | `Dsqt::Bridge` |
| N/A (monolithic) | `Dsqt::Waffles` |
| N/A (monolithic) | `Dsqt::TouchEngine` |
| N/A (monolithic) | `Dsqt::Spout` |

## Quick Reference: Old vs New QML Imports

| Old Import | New Import |
|---|---|
| `import Dsqt` | `import Dsqt.Core` |
| `import WafflesUx` | `import Dsqt.Waffles` |
| `import TouchEngineQt` | `import Dsqt.TouchEngine` |
| N/A | `import Dsqt.Spout` |

## Quick Reference: CMake Variables Set by find_package(Dsqt)

| Variable | Description |
|---|---|
| `DSQT_INSTALL_PREFIX` | Root of the DsQt installation |
| `DSQT_LIB_DIR` | Library files (`.lib`) |
| `DSQT_INCLUDE_DIR` | Header files |
| `DSQT_BIN_DIR` | Binaries and DLLs |
| `DSQT_QML_DIR` | QML module files |
| `DSQT_QML_IMPORT_PATH` | Path for Qt Creator QML resolution |
| `Dsqt_INCLUDES` | Include directories for `target_include_directories` |
| `TouchEngineQt_INCLUDES` | TouchEngine-specific include directories |

## Troubleshooting

**CMake cannot find Dsqt:** Check the active source mode. Registry/local builds
need the helpers before `project()` and a successful vcpkg installation. Ordinary
installed-library builds need a compatible installation and its explicit config
path. Inspect `Dsqt_DIR` in the actual build cache.

**Linker errors about missing symbols:** Verify you are linking all required `Dsqt::` targets and that you've added the `Q_IMPORT_QML_PLUGIN` macros in `main.cpp`.

**QML module not found at runtime:** Check that `IMPORT_PATH "${DSQT_QML_IMPORT_PATH}"` is set in your `qt_add_qml_module` call.

**TouchEngine.dll not found:** Ensure the package contains TouchEngine and the
layout-aware deployment helpers run for the application target and installation.

**Need to change package versions or sources:** Use the [switching guide](switching_dsqt_packages.md).
For machine-specific paths, use [CMake user presets](cmake_user_presets.md); shared
team configuration belongs in `CMakePresets.json`.
