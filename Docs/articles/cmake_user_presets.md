# CMake user presets for DsQt applications

Use **ProjectCloner + Published vcpkg package** for a new application. It generates
presets for the selected package source and compatible Qt kits. For an existing
app, use [Switching DsQt packages, versions, and Qt kits](switching_dsqt_packages.md)
to update package pins, add a source mode, or integrate the current CMake helpers.
This guide focuses on machine-specific paths and preset inheritance.

## Shared settings versus local settings

`CMakePresets.json` contains the team's shared build configuration and belongs in
Git. `CMakeUserPresets.json`, beside it, contains local paths and additional
presets; keep that file ignored. CMake reads both files together. A configure
preset can inherit from another **configure preset**, including a hidden one.
Its name must be unique across both files. A build preset refers to a configure
preset through `configurePreset`; it cannot serve as the parent of one.
See the [CMake presets reference](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html).

Inspect your app before choosing names:

```powershell
cmake --list-presets
cmake --list-presets=build
Get-Content CMakePresets.json
```

Typical generated configure names for Qt 6.12.0 are `ninja-vcpkg-6.12.0` for the
registry, `ninja-vcpkg-local` for a local source package, and `ninja-6.12.0` for an
ordinary installed library. Only the source chosen in ProjectCloner is generated.
Visual Studio preset names depend on the discovered installation. Older raw
template presets are historical examples, not proof that the current package
supports their Qt versions. Current Core and Waffles require Qt 6.12.0 or newer
compatible SDK, including Canvas Painter.

## Registry package: Qt installed outside C:/Qt

If your shared file has `ninja-vcpkg-6.12.0` and the kit is installed at
`D:/Qt/6.12.0/msvc2022_64`, use this complete `CMakeUserPresets.json`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "user-registry-d-drive",
      "inherits": "ninja-vcpkg-6.12.0",
      "binaryDir": "${sourceDir}/build/user-registry-d-drive",
      "cacheVariables": {
        "DSQT_REGISTRY_QT_ROOT": "D:/Qt",
        "DSQT_REGISTRY_QT_VERSION": "6.12.0"
      }
    }
  ],
  "buildPresets": [
    { "name": "user-registry-d-drive-debug", "configurePreset": "user-registry-d-drive", "configuration": "Debug" },
    { "name": "user-registry-d-drive-release", "configurePreset": "user-registry-d-drive", "configuration": "Release" }
  ]
}
```

The root is `D:/Qt`, not the complete kit directory. The registry helper sets
`CMAKE_PREFIX_PATH`, `Qt6_DIR`, the triplet, and `Dsqt_DIR` from this selection.
Do not add a `Documents/DsQt` prefix to a registry preset. To change the Qt version,
choose an installed compatible version, update `DSQT_REGISTRY_QT_VERSION`, and
give the preset and its build directory new names. This does not select a new
DsQt version; update the manifest and registry pins for that separate change.

From the app directory in an x64 MSVC developer shell, with `VCPKG_ROOT` set:

```powershell
cmake --preset user-registry-d-drive
cmake --build --preset user-registry-d-drive-release
cmake --install build/user-registry-d-drive --config Release
```

## Local source package: another package root

By default, the local-package helper discovers `%LOCALAPPDATA%\DsQt\vcpkg`.
Successful `build_and_install.bat -vcpkg` runs update that store's default;
ProjectCloner's local selection reads it and records the store in the generated
preset. Selecting an exact snapshot directory pins that snapshot instead.
To use another store or snapshot, explicitly
override that path as below. The example `D:/DsQt Packages` must already contain
a successful publication made with `-publish-root "D:\DsQt Packages"`.

If the shared file has `ninja-vcpkg-local`, a local override can point at a
completed package produced by `build_and_install.bat -vcpkg`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "user-local-package",
      "inherits": "ninja-vcpkg-local",
      "binaryDir": "${sourceDir}/build/user-local-package",
      "cacheVariables": {
        "DSQT_LOCAL_PACKAGE_ROOT": "D:/DsQt Packages"
      }
    }
  ],
  "buildPresets": [
    { "name": "user-local-package-release", "configurePreset": "user-local-package", "configuration": "Release" }
  ]
}
```

The package's `dsqt-local-package.json` chooses the exact Qt kit and dependency
baseline. A consumer prefix override cannot substitute a different Qt kit.
Rebuild the local package for another kit. If both examples are needed, merge
their configure/build entries into one user file; do not create duplicate keys.

For this example, publish from a Windows Command Prompt, replacing the checkout,
vcpkg, Qt, and store paths as needed:

```bat
set "VCPKG_ROOT=C:\vcpkg"
C:\dev\DsQt\Library\build_and_install.bat -vcpkg -qt 6.12.0 -publish-root "D:\DsQt Packages"
```

From the app directory in the matching MSVC developer shell:

```powershell
cmake --fresh --preset user-local-package
cmake --build --preset user-local-package-release
cmake --install build/user-local-package --config Release
```

These preset/build directory names match the JSON above; substitute the names
in your application. Fresh configuration is required when a store default moves
to a different snapshot or Qt kit.

## Ordinary installed library

In this mode, both `DSQT_USE_REGISTRY_VCPKG` and `DSQT_USE_LOCAL_VCPKG` are off.
Point `CMAKE_PREFIX_PATH` at the matching Qt kit and DsQt installation, and use
`Dsqt_DIR` to identify that installation explicitly. For example, the relevant
cache-variable fragment is:

```json
{
  "DSQT_USE_REGISTRY_VCPKG": "OFF",
  "DSQT_USE_LOCAL_VCPKG": "OFF",
  "CMAKE_PREFIX_PATH": "C:/Qt/6.12.0/msvc2022_64;D:/libs/DsQt-qt6.12.0",
  "Dsqt_DIR": "D:/libs/DsQt-qt6.12.0/lib/cmake/Dsqt"
}
```

Use a separate build directory and ensure that the installation contains the
requested modules and configuration. The [switching guide](switching_dsqt_packages.md#3-choose-the-source-in-presets)
contains complete presets for all three modes, independent of generated names.

## Building the DsQt library itself

The library's own `Library/CMakePresets.json` is a different project. Its
configure preset is `ninja`; it does not use the app's registry-consumer preset
to build itself. The easiest path is `Library/build_and_install.bat -qt 6.12.0`.
For manual library work, a local user preset can inherit `ninja`, set the exact
Qt prefix, and use its own build directory. Replace historical Qt versions in
`Library/CMakeUserPresets.example.json` with your actual compatible SDK.

## Build configuration and stale caches

Ninja Multi-Config uses the build preset's `configuration` to select Debug or
Release; changing `CMAKE_BUILD_TYPE` does not select its output configuration.
Use the same configuration in `cmake --install ... --config ...`. Run the
installed app with its matching Qt/CRT/plugins or the project's launch setup.

For another generator, use a new configure preset and build directory with an
installed generator from `cmake --help`; do not assume Visual Studio 2022 is
installed merely because the Qt kit is named `msvc2022_64`. Ninja builds need
the matching x64 compiler environment in the shell or IDE configuration.

Prefer separate directories when changing source mode, Qt, architecture, or
compiler. `cmake --fresh --preset <name>` resets CMake's cache for a preset, but
does not remove all dependencies or deployed files. Verify `Qt6_DIR`, `Dsqt_DIR`,
and the active source flags in `CMakeCache.txt` if the result is unexpected.
