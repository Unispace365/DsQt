# Migrating from DsQt Subdirectory to DsQt Library

This guide walks through converting a project that uses DsQt via `add_subdirectory()` to one that consumes DsQt as a pre-built, installed library via `find_package()`.

## Automated Migration (Recommended)

A Python script automates most of the migration steps described below. It parses your existing `CMakeLists.txt`, generates updated build files, patches `main.cpp`, rewrites QML imports, and copies scaffolding from `ClonerSource`.

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

After running the script, do a clean rebuild (delete `build/`, re-configure, build). See the manual steps below for details on what each change does and for troubleshooting.

---

## Manual Migration Steps

The following sections describe each migration step in detail. If you used the automated script above, these serve as a reference for understanding and troubleshooting the changes.

## Prerequisites

- Visual Studio 2022
- Qt 6.10.x installed (e.g. `C:/Qt/6.10.1/msvc2022_64`)
- vcpkg installed and `VCPKG_ROOT` environment variable set
- The DsQt repository cloned locally

## Step 1: Build and Install the DsQt Library

Before your project can consume DsQt as a library, you need to build and install it once.

1. Open the DsQt **Library** project in Qt Creator:
   - File > Open > navigate to `DsQt/Library/CMakeLists.txt`
2. Select a configure preset (e.g. **VS2022 x64 - Qt 6.10.1**).
3. Build the project (Build > Build All).
4. Install the project (Build > Install, or run `cmake --install` from the command line).

The library installs to `%USERPROFILE%\Documents\DsQt` by default. This location is configured in `Library/CMakePresets.json` and is where downstream projects will find it.

> **Tip:** You only need to rebuild and reinstall DsQt when the library itself changes. Your application projects reference the installed copy.

## Step 2: Copy Project Scaffolding from ClonerSource

Several files in the `Examples/ClonerSource` directory serve as the reference template for a library-based project. Copy the following files into your project root, replacing existing versions where noted:

### Files to Copy

| Source (from `Examples/ClonerSource/`) | Destination (your project root) | Notes |
|---|---|---|
| `CMakeLists.txt` | `CMakeLists.txt` | **Replace entirely.** This is the most important change. See Step 3 for details. |
| `CMakePresets.json` | `CMakePresets.json` | **Replace entirely.** Adds the DsQt install path to `CMAKE_PREFIX_PATH`. Do not edit directly — use `CMakeUserPresets.json` for local overrides (see Step 6). |
| `vcpkg.json` | `vcpkg.json` | **Replace or merge.** Provides `tomlplusplus`, `glm`, and `pkgconf` dependencies. |
| `main.cpp` | `main.cpp` | **Replace or use as reference.** Contains the new `Q_IMPORT_QML_PLUGIN` macros and module-based loading. |
| `cmake/app.rc.in` | `cmake/app.rc.in` | Windows resource file template. Copy if you don't already have one. |
| `resource1.h` | `resource1.h` | Windows resource header. Copy if you don't already have one. |
| `VERSION.txt` | `VERSION.txt` | Version source of truth. Create with your version (e.g. `1.0.0`). |
| `.gitignore` | `.gitignore` | **Merge** with your existing `.gitignore`. |
| `project-ci.yml` | `project-ci.yml` | CI configuration template. Copy and update the `name` and `qt_modules` fields. |
| `settings/` | `settings/` | Copy the full directory if starting fresh, or merge individual `.toml` files. |

> **Alternative:** Instead of copying manually, you can use the **ProjectCloner** tool (`Tools/ProjectCloner/`) to generate a fresh project from the ClonerSource template, then migrate your QML and C++ source files into it.

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
# Find Dsqt from installed package (CMAKE_PREFIX_PATH set via CMakePresets.json)
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

**After (library approach):**
```cmake
target_link_libraries(${DS_EXE_NAME}
    PRIVATE
        Dsqt::Core
        Dsqt::Bridge
        Dsqt::TouchEngine
        Dsqt::Waffles

        Qt6::Concurrent
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::ShaderTools
        Qt6::GuiPrivate
        Qt6::Sql

        PkgConfig::tomlplusplus
        glm::glm
)
```

### Update: target_include_directories

**Before:**
```cmake
target_include_directories(${DS_EXE_NAME} PRIVATE ${Dsqt_INCLUDES})
```

**After:**
```cmake
target_include_directories(${DS_EXE_NAME} PRIVATE ${Dsqt_INCLUDES} ${TouchEngineQt_INCLUDES})
```

These variables are set automatically by `DsqtConfig.cmake` when `find_package(Dsqt)` runs.

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
        Dsqt::Core
        Dsqt::Bridge
        Dsqt::TouchEngine
        Dsqt::Waffles
        QtQuick
)
```

### Add: TouchEngine DLL handling (if using TouchEngine)

```cmake
# TouchEngine SDK DLL location (from installed package, config-specific subdirectory)
set(TOUCHENGINE_DLL "${DSQT_BIN_DIR}/$<CONFIG>/TouchEngine.dll")

if(WIN32)
    add_custom_command(TARGET ${DS_EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${TOUCHENGINE_DLL}"
            $<TARGET_FILE_DIR:${DS_EXE_NAME}>
        COMMENT "Copying TouchEngine.dll to output directory"
    )
endif()
```

### Add: Deploy script for TouchEngine DLL (if using TouchEngine)

```cmake
if(WIN32)
    install(FILES "${DSQT_BIN_DIR}/$<CONFIG>/TouchEngine.dll"
        DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
endif()
```

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

Each `DSQT_HAS_<Module>` definition is set automatically when the corresponding component is found by `find_package(Dsqt)`. In your `CMakeLists.txt`, add this pattern after `find_package`:

```cmake
foreach(_comp Core Bridge Waffles TouchEngine Spout)
    if(Dsqt_${_comp}_FOUND)
        target_compile_definitions(${DS_EXE_NAME} PRIVATE DSQT_HAS_${_comp})
    endif()
endforeach()
```

This makes each module truly optional — if a component isn't found (or isn't requested), the corresponding plugin import is simply skipped.

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

Your project needs the DsQt install location in `CMAKE_PREFIX_PATH`. The generated `CMakePresets.json` already includes this. **Do not edit `CMakePresets.json` directly** — it is managed by the migration script and may be regenerated. Instead, use `CMakeUserPresets.json` for any local overrides.

### CMakePresets.json (generated, checked into source control)

The migration script generates a `CMakePresets.json` with sensible defaults, including the DsQt install path:

```json
{
  "name": "qt-6.10.1",
  "hidden": true,
  "inherits": "vcpkg-base",
  "cacheVariables": {
    "CMAKE_PREFIX_PATH": "C:/Qt/6.10.1/msvc2022_64;$env{USERPROFILE}/Documents/DsQt"
  }
}
```

This file should be committed to version control so all team members share the same base configuration.

### CMakeUserPresets.json (local overrides, not checked in)

To customize paths, add Qt versions, or override variables for your local environment, create a `CMakeUserPresets.json` in your project root. CMake automatically merges it with `CMakePresets.json`. This file should be listed in `.gitignore`.

**Example — override the DsQt install path and add a custom Qt location:**

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "my-local",
      "inherits": "vs2022-6.10.1-debug",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "D:/Qt/6.10.1/msvc2022_64;D:/libs/DsQt"
      }
    }
  ]
}
```

**Example — add a Qt version not in the base presets:**

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "qt-6.11.0",
      "hidden": true,
      "inherits": "vcpkg-base",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "C:/Qt/6.11.0/msvc2022_64;$env{USERPROFILE}/Documents/DsQt"
      }
    },
    {
      "name": "vs2022-6.11.0-debug",
      "displayName": "VS2022 x64 Debug - Qt 6.11.0",
      "inherits": ["qt-6.11.0", "vs2022-base"],
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    }
  ]
}
```

> **Tip:** Make sure your `.gitignore` includes `CMakeUserPresets.json`. The migration script and ClonerSource `.gitignore` already include it.

## Step 7: Remove the DS_QT_PLATFORM_100 Environment Variable

The subdirectory approach relied on the `DS_QT_PLATFORM_100` environment variable to locate the DsQt source tree. This is no longer needed. You can remove it from your system environment variables.

## Step 8: Clean and Rebuild

1. Delete your existing `build/` directory.
2. Re-configure the project using your CMake preset.
3. Build and verify everything compiles and links correctly.

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

**CMake can't find Dsqt:** Make sure you've built and installed the DsQt Library (Step 1) and that `CMAKE_PREFIX_PATH` in your `CMakePresets.json` includes the install location.

**Linker errors about missing symbols:** Verify you are linking all required `Dsqt::` targets and that you've added the `Q_IMPORT_QML_PLUGIN` macros in `main.cpp`.

**QML module not found at runtime:** Check that `IMPORT_PATH "${DSQT_QML_IMPORT_PATH}"` is set in your `qt_add_qml_module` call.

**TouchEngine.dll not found:** Ensure the post-build copy command is in your CMakeLists.txt and that the DsQt library was installed with TouchEngine support.

**Need to customize paths or add Qt versions:** Don't edit `CMakePresets.json` — create a `CMakeUserPresets.json` in your project root with local overrides. CMake merges both files automatically. See Step 6 for examples.
