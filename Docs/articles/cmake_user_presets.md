# Enabling Additional Build Presets with CMakeUserPresets.json

The `CMakePresets.json` in both the **DsQt Library** and every **project created via the Project Cloner tool** exposes a single default configure preset — the Ninja Multi-Config build for the current Qt version (e.g., `ninja-6.10.2`). All other presets — older Qt versions, the Visual Studio 2022 generator, and custom paths — are defined but marked `hidden` so they stay out of the way for most builds.

The preset naming convention follows the pattern `<generator>-<qt-version>` (e.g., `ninja-6.10.2`, `vs2022-6.10.1`). As Qt versions are updated over time, new presets are added and the previous current version is hidden. The examples in this document use `6.10.2` as the current version — substitute whatever version suffix appears as the visible preset in your copy of `CMakePresets.json`.

`CMakeUserPresets.json` is a parallel file you create alongside `CMakePresets.json` in your project root. CMake and all major IDEs load it automatically and merge it with the shared presets. Because it is **not committed to source control** (the project `.gitignore` already excludes it), each developer can maintain their own local configuration without affecting the team.

> **Do not edit `CMakePresets.json` to accommodate your local setup.** That file is committed to source control and shared across the team. All local customisation belongs in `CMakeUserPresets.json`.

## Table of Contents

1. [How Inheritance Works](#how-inheritance-works)
2. [Creating the File](#creating-the-file)
3. [Scenario: Visual Studio 2022 Generator](#scenario-visual-studio-2022-generator)
4. [Scenario: Older Qt Version](#scenario-older-qt-version)
5. [Scenario: Qt at a Non-Standard Path](#scenario-qt-at-a-non-standard-path)
6. [Scenario: Custom DsQt Install Location](#scenario-custom-dsqt-install-location)
7. [Adding Build Presets](#adding-build-presets)
8. [Using with the DsQt Library](#using-with-the-dsqt-library)
9. [IDE Integration](#ide-integration)

---

## How Inheritance Works

User presets can inherit from any named preset in either file — including hidden ones. The hidden presets in `CMakePresets.json` exist specifically to be composed in `CMakeUserPresets.json`. The available base presets follow a consistent pattern across both the Library and projects created via the Project Cloner tool:

| Preset pattern | What it provides |
|---|---|
| `vcpkg-base` | vcpkg toolchain file via `$env{VCPKG_ROOT}` |
| `qt-X.Y.Z` | `CMAKE_PREFIX_PATH` pointing at the standard Qt and DsQt install locations for that version |
| `vs2022-X.Y.Z` | Visual Studio 17 2022, x64, plus the matching Qt paths |
| `ninja-base` | Ninja Multi-Config generator with MSVC (`cl.exe`) compilers |
| `ninja-X.Y.Z` | Ninja + matching Qt paths for that version |

Check the `configurePresets` array in your `CMakePresets.json` to see which specific version presets are available — the list will grow as new Qt versions are added. Any preset marked `"hidden": true` can be inherited in your user presets.

> **Preset names must be unique across both files.** CMake treats `CMakePresets.json` and `CMakeUserPresets.json` as a single merged set — a name that already exists in `CMakePresets.json` cannot be reused in `CMakeUserPresets.json`. Always give your user presets distinct names (the examples below use a `local-` prefix as a convention).

---

## Creating the File

Create `CMakeUserPresets.json` at the root of your project (the same directory as `CMakePresets.json`):

```json
{
  "version": 6,
  "configurePresets": [],
  "buildPresets": []
}
```

Replace the empty arrays with the examples below as needed.

---

## Scenario: Visual Studio 2022 Generator

The shipped `vs2022-X.Y.Z` configure presets are hidden. Create a new configure preset that inherits from the hidden one, giving it a unique name, then add matching build presets:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-vs2022-6.10.2",
      "displayName": "local-vs2022-6.10.2",
      "inherits": "vs2022-6.10.2"
    }
  ],
  "buildPresets": [
    {
      "name": "local-vs2022-6.10.2-debug",
      "displayName": "Debug",
      "configurePreset": "local-vs2022-6.10.2",
      "configuration": "Debug"
    },
    {
      "name": "local-vs2022-6.10.2-release",
      "displayName": "Release",
      "configurePreset": "local-vs2022-6.10.2",
      "configuration": "Release"
    },
    {
      "name": "local-vs2022-6.10.2-relwithdebinfo",
      "displayName": "RelWithDebInfo",
      "configurePreset": "local-vs2022-6.10.2",
      "configuration": "RelWithDebInfo"
    }
  ]
}
```

After saving, your IDE will show **VS2022 x64 - Qt 6.10.2** alongside the default Ninja preset.

> Replace `6.10.2` with whichever `vs2022-X.Y.Z` preset exists in your `CMakePresets.json`.

---

## Scenario: Older Qt Version

All `ninja-X.Y.Z` and `vs2022-X.Y.Z` configure presets except the current one are hidden but fully functional. Create a new preset that inherits from the hidden one and provide your own build presets pointing at it:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-ninja-6.10.1",
      "displayName": "local-ninja-6.10.1",
      "inherits": "ninja-6.10.1"
    }
  ],
  "buildPresets": [
    {
      "name": "local-ninja-6.10.1-debug",
      "displayName": "Debug",
      "configurePreset": "local-ninja-6.10.1",
      "configuration": "Debug"
    },
    {
      "name": "local-ninja-6.10.1-release",
      "displayName": "Release",
      "configurePreset": "local-ninja-6.10.1",
      "configuration": "Release"
    },
    {
      "name": "local-ninja-6.10.1-relwithdebinfo",
      "displayName": "RelWithDebInfo",
      "configurePreset": "local-ninja-6.10.1",
      "configuration": "RelWithDebInfo"
    }
  ]
}
```

> The `local-` prefix here is just a convention to avoid colliding with names already defined in `CMakePresets.json`. You can use any prefix or suffix that makes the name unique.

---

## Scenario: Qt at a Non-Standard Path

The `qt-X.Y.Z` base presets assume Qt is installed at `C:/Qt/<version>/msvc2022_64` — the default path the Qt Maintenance Tool suggests on Windows. It is common to install Qt elsewhere: to a secondary drive to keep the system drive lean, to a team-wide network share, or simply because a different root was chosen during Qt Maintenance Tool setup.

### Example: Qt on a secondary drive

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-qt-6.10.2",
      "hidden": true,
      "inherits": "vcpkg-base",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "D:/Qt/6.10.2/msvc2022_64;$env{USERPROFILE}/Documents/DsQt"
      }
    },
    {
      "name": "local-ninja-6.10.2",
      "displayName": "local-ninja-6.10.2",
      "inherits": ["local-qt-6.10.2", "ninja-base"],
      "binaryDir": "${sourceDir}/build/ninja-6.10.2"
    }
  ],
  "buildPresets": [
    {
      "name": "local-ninja-6.10.2-debug",
      "displayName": "Debug",
      "configurePreset": "local-ninja-6.10.2",
      "configuration": "Debug"
    },
    {
      "name": "local-ninja-6.10.2-release",
      "displayName": "Release",
      "configurePreset": "local-ninja-6.10.2",
      "configuration": "Release"
    },
    {
      "name": "local-ninja-6.10.2-relwithdebinfo",
      "displayName": "RelWithDebInfo",
      "configurePreset": "local-ninja-6.10.2",
      "configuration": "RelWithDebInfo"
    }
  ]
}
```

### Example: Qt installed to a custom directory on C:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-qt-6.10.2",
      "hidden": true,
      "inherits": "vcpkg-base",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "C:/dev/tools/Qt/6.10.2/msvc2022_64;$env{USERPROFILE}/Documents/DsQt"
      }
    },
    {
      "name": "local-ninja-6.10.2",
      "displayName": "local-ninja-6.10.2",
      "inherits": ["local-qt-6.10.2", "ninja-base"],
      "binaryDir": "${sourceDir}/build/ninja-6.10.2"
    }
  ],
  "buildPresets": [
    {
      "name": "local-ninja-6.10.2-debug",
      "displayName": "Debug",
      "configurePreset": "local-ninja-6.10.2",
      "configuration": "Debug"
    },
    {
      "name": "local-ninja-6.10.2-release",
      "displayName": "Release",
      "configurePreset": "local-ninja-6.10.2",
      "configuration": "Release"
    },
    {
      "name": "local-ninja-6.10.2-relwithdebinfo",
      "displayName": "RelWithDebInfo",
      "configurePreset": "local-ninja-6.10.2",
      "configuration": "RelWithDebInfo"
    }
  ]
}
```

In both cases only the first segment of `CMAKE_PREFIX_PATH` changes — the rest of the preset structure is identical.

> **`CMAKE_PREFIX_PATH` in Project Cloner projects requires two entries** separated by a semicolon: the Qt kit path and the DsQt install path. DsQt defaults to `$env{USERPROFILE}/Documents/DsQt` (the default `CMAKE_INSTALL_PREFIX` from the Library build). See [Using with the DsQt Library](#using-with-the-dsqt-library) for the difference when working in the Library itself.

---

## Scenario: Custom DsQt Install Location

If you built the DsQt Library and installed it somewhere other than `%USERPROFILE%\Documents\DsQt`, update the second path in `CMAKE_PREFIX_PATH`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-qt-6.10.2",
      "hidden": true,
      "inherits": "vcpkg-base",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "C:/Qt/6.10.2/msvc2022_64;C:/dev/DsQtInstall"
      }
    },
    {
      "name": "local-ninja-6.10.2",
      "displayName": "local-ninja-6.10.2",
      "inherits": ["local-qt-6.10.2", "ninja-base"],
      "binaryDir": "${sourceDir}/build/ninja-6.10.2"
    }
  ]
}
```

---

## Adding Build Presets

Build presets tell IDEs which configurations (Debug, Release, RelWithDebInfo) are available for a given configure preset. The pattern is always the same — one entry per configuration:

```json
"buildPresets": [
  {
    "name": "<preset-name>-debug",
    "displayName": "Debug",
    "configurePreset": "<preset-name>",
    "configuration": "Debug"
  },
  {
    "name": "<preset-name>-release",
    "displayName": "Release",
    "configurePreset": "<preset-name>",
    "configuration": "Release"
  },
  {
    "name": "<preset-name>-relwithdebinfo",
    "displayName": "RelWithDebInfo",
    "configurePreset": "<preset-name>",
    "configuration": "RelWithDebInfo"
  }
]
```

Replace `<preset-name>` with the `name` of your configure preset. Build preset names must also be unique across both files.

---

## Using with the DsQt Library

The DsQt Library's own `CMakePresets.json` follows the exact same pattern — one visible Ninja preset for the current Qt version, all others hidden — so every scenario in this document applies there too.

The one structural difference is in `CMAKE_PREFIX_PATH`. Projects created via the Project Cloner tool need **two** entries (Qt + the DsQt install location) because they consume DsQt as an installed package. When working in the Library itself you are building DsQt, not consuming it, so `CMAKE_PREFIX_PATH` only needs the Qt path. The Library's hidden `qt-X.Y.Z` presets already reflect this:

```json
{
  "name": "qt-6.10.2",
  "hidden": true,
  "inherits": "vcpkg-base",
  "cacheVariables": {
    "CMAKE_PREFIX_PATH": "C:/Qt/6.10.2/msvc2022_64",
    "CMAKE_INSTALL_PREFIX": "$env{USERPROFILE}/Documents/DsQt"
  }
}
```

So when writing a `CMakeUserPresets.json` for the Library, inherit from `vcpkg-base` and set only the Qt path, keeping `CMAKE_INSTALL_PREFIX` to control where the Library gets installed:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-qt-6.10.2",
      "hidden": true,
      "inherits": "vcpkg-base",
      "cacheVariables": {
        "CMAKE_PREFIX_PATH": "D:/SDKs/Qt/6.10.2/msvc2022_64",
        "CMAKE_INSTALL_PREFIX": "$env{USERPROFILE}/Documents/DsQt"
      }
    },
    {
      "name": "local-ninja-6.10.2",
      "displayName": "local-ninja-6.10.2",
      "inherits": ["local-qt-6.10.2", "ninja-base"],
      "binaryDir": "${sourceDir}/build/ninja-6.10.2"
    }
  ]
}
```

---

## IDE Integration

### Visual Studio Code (CMake Tools extension)

The CMake Tools extension picks up `CMakeUserPresets.json` automatically. After saving the file, open the Command Palette and run **CMake: Select Configure Preset** — your user presets appear alongside the shipped ones.

### Visual Studio 2022

Visual Studio reads both preset files when the project is opened. User presets appear in the configuration dropdown in the toolbar. If they do not appear after saving, right-click the project root in Solution Explorer and choose **Delete Cache and Reconfigure**.

### Qt Creator

After adding or modifying `CMakeUserPresets.json`, go to **Build > Run CMake** or re-open the project to pick up the new presets.

### Command Line

```bat
:: List all available presets (shipped + user)
cmake --list-presets

:: Configure
cmake --preset local-ninja-6.10.2

:: Build
cmake --build --preset local-ninja-6.10.2-debug
```
