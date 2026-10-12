# DsQt

## Description (the what and the why)

DsQt (Dee Ess Cute) is a framework for interactive applications built on top of the [Qt framework](https://qt.io/)

DsQt is a fully featured windows framework for rapid production of multimedia applications.
It is a replacement for DS Cinder. It Relies on Qt and QML for most features  (Multimedia, Fonts, 3D rendering)
but adds comprensive configuration data modeling and integration with our
CMS backend for rapid prototyping and developemnt.

## Documentation

Documentation is compiled by Doxygen and can be found here:
<https://unispace365.github.io/DsQt/>

For Waffles applications, start with the [Waffles overview](Docs/waffles-overview.md)
and [consolidated implementation guide](Docs/waffles-implementation-guide.md).
The [Source Tour](Docs/waffles-tour/index.html) covers individual components;
open its HTML locally for the styled walkthrough.

## Getting Started

> This version of DsQt has been built and is targeted at Windows. We hope to have Linux and Apple builds.

### Windows

#### Install Visual Studio 2022 - Community Edition

1. Download here: <https://www.visualstudio.com/>
2. Install it!
        - Make sure to select C++ development and ensure you have the latest windows SDK and visual studio updates!

#### Install a Qt 6.12 SDK for this branch

The current Core and Waffles modules require Qt 6.12, including Qt Canvas Painter
and the `QtCanvas2D` QML module. Older Qt versions in historical presets and
templates must be overridden before building this branch. Use the same Qt SDK,
architecture, and build configuration for DsQt and its consuming application.

1. Create a Qt.io account if you don't have one. <https://login.qt.io/register>
2. Go to <https://www.qt.io/download-qt-installer-oss> and download Qt for x64
3. Run the online installer.
4. You don't need to install everything. In fact I'd strongly recommend against it. But also it's hard to know what your going to need. I think the following are the important ones:

    * Extensions:
        * Qt PDF:
            * A package compatible with the selected Qt 6.12 SDK:
                * MSVC 2022 x64
        * Qt WebEngine:
            * A package compatible with the selected Qt 6.12 SDK:
                * MSVC 2022 x64
    * Qt:
        * Qt 6.12:
            * MSVC 2022 64-bit
            * Additional Libraries:
                * Qt Canvas Painter (including QtCanvas2D)
                * Qt Multimedia
                * Qt Image Formats
                * Qt Language Server
                * Qt Positioning
                * Qt Virtual Keyboard
                * Qt Quick 3d
                * Qt Quick Effect Maker
                * Qt WebChannel
                * Qt WebSockets
                * (Everything else you can get just to be safe)
    * Qt Creator (check all boxes)
5. Depending on what you selected this can take a while to download (hours sometimes).

#### Install cmake

On Windows, download the latest cmake x64 installer (.msi) from https://cmake.org/download/

#### Install vcpkg

1. Clone vcpkg: `git clone https://github.com/microsoft/vcpkg.git C:\vcpkg`
⚠️ Note that if vcpkg is/was already on your machine you may need to do a `git pull` before running the bootstrap script ⚠️
2. Run bootstrap: `cd C:\vcpkg && .\bootstrap-vcpkg.bat`
3. Add environment variable `VCPKG_ROOT` pointing to `C:\vcpkg`:
    1. Open the Start menu and type "Advanced System Settings"
    2. Click "Environment Variables"
    3. Under "System variables", select "New..."
    4. Variable name: `VCPKG_ROOT`
    5. Variable value: `C:\vcpkg`
    6. Ok, Ok, Ok

#### Updating vcpkg Baseline

If you encounter build errors related to `tomlplusplus` (e.g. missing headers, version
conflicts, or compilation failures), your vcpkg baseline is likely too old. Update it:

```bash
cd C:\vcpkg
git pull
.\bootstrap-vcpkg.bat
```

Then update the baseline hash in your project's `vcpkg.json`:

```bash
vcpkg x-update-baseline
```

This updates the `builtin-baseline` field to the latest vcpkg commit, which ensures
you get compatible package versions.

#### Download and Configure DsQt

1. Download and install Git and ensure you have git-lfs available
2. Run `git clone https://github.com/Unispace365/DsQt.git`
3. `cd DsQt`
4. Run: `git lfs install`
5. Run: `git lfs fetch --all`
6. Add platform environment variable (used by ProjectCloner to locate templates)
    1. Open the Start menu and type "Advanced System Settings"
    2. Click "Environment Variables"
    3. Under "System variables", select "New..."
    4. Variable name: `DS_QT_PLATFORM_100`
    5. Variable value: the path of this downloaded repo (e.g `C:\Users\YourNameHere\Documents\DsQt`)
    6. Ok, Ok, Ok

#### Build the DsQt Library

**Preferred (command-line):**

1. `cd Library`
2. Choose an installed Qt 6.12 SDK. If using `CMakeUserPresets.example.json`, replace
   its historical Qt version/path examples with your SDK in `CMakeUserPresets.json`.
3. Run `build_and_install.bat -qt "C:\Qt\6.12.0\msvc2022_64"` (substitute your SDK path)
   — configures, builds Debug + Release, and installs to
   `%USERPROFILE%\Documents\DsQt`
4. For a standard `C:\Qt\<version>\msvc2022_64` installation, `-qt 6.12.0` is shorthand.
5. Add `-tools` to also build and install ProjectCloner + ClonerSource.

**Alternative (Qt Creator):**

1. Open Qt Creator
2. File > Open File or Project, navigate to `DsQt/Library/CMakeLists.txt`
3. Qt Creator will detect the CMakePresets.json — select the `ninja` preset (or a
   version-pinned preset from your `CMakeUserPresets.json`) and ensure it resolves Qt 6.12.
4. Click Configure Project, then Build > Install

#### Build and Run ProjectCloner

**Preferred (command-line):**

1. `cd Library`
2. Run `build_and_install.bat -qt "C:\Qt\6.12.0\msvc2022_64" -tools` with your SDK path — this builds and installs ProjectCloner and
   copies the ClonerSource template to `%USERPROFILE%\Documents\DsQt\Tools\`
3. Run: `%USERPROFILE%\Documents\DsQt\Tools\ProjectCloner\bin\appProjectCloner.exe`

**Alternative (Qt Creator):**

1. Open Qt Creator
2. File > Open File or Project, navigate to `DsQt/Tools/ProjectCloner/CMakeLists.txt`
3. Select the `ninja` preset (or a version-pinned preset) and click Configure Project
4. Build the project (Ctrl+B), then Build > Install
5. Run: `%DS_QT_PLATFORM_100%\Tools\deploy\ProjectCloner\bin\appProjectCloner.exe`

## Creating a New Project

Always create a new DsQt application with **ProjectCloner**. AssetViewer and
ECPresenter are reference implementations for integration patterns. If you are
adding Waffles to an existing DsQt application, follow the
[existing-project path](#adding-waffles-to-an-existing-dsqt-project).

### Using ProjectCloner

1. [Build and install ProjectCloner](#build-and-run-projectcloner), then run
   `%USERPROFILE%\Documents\DsQt\Tools\ProjectCloner\bin\appProjectCloner.exe`
   for the default installation prefix.
2. Select destination folder
3. Enter project name (alphanumeric and underscores only)
4. Click "Clone"

The tool creates a complete project from the ClonerSource template with:

* CMakeLists.txt configured for DsQt
* CMakePresets.json for easy building
* QML application structure
* Settings files (TOML configuration)
* Sample assets and fonts

### Building Your New Project

**Command-line (CMake presets):**

1. `cd <your-project>`
2. Run `cmake --list-presets` and `cmake --list-presets=build` to inspect the generated presets.
3. Add a local Qt 6.12 preset if the template still lists older SDKs. Set its
   `CMAKE_PREFIX_PATH` to your Qt SDK and installed DsQt package; see the guide below.
4. Configure with `cmake --preset <configure-preset>` and build with
   `cmake --build --preset <build-preset>`, substituting actual preset names.
5. Install with `cmake --install <build-directory> --config Release` (or Debug).
6. Run from the installed `bin` directory reported by CMake. Its location depends
   on the application's install rules and build configuration.

To see all available presets: `cmake --list-presets`

If your Qt is not installed under `C:\Qt\` or you need a different preset, see the
[CMake User Presets Guide](Docs/articles/cmake_user_presets.md).

**Qt Creator:**

1. Open Qt Creator
2. File > Open File or Project, navigate to your project's `CMakeLists.txt`
3. Select a preset configured for your Qt 6.12 SDK and installed DsQt package,
   then click Configure Project.
4. Go to **Projects** > **Deploy Settings** and click **Add Deploy Step** > **CMake install**
5. Go to **Projects** > **Run Settings** and set the **Executable** to the installed
   application and the **Working directory** to its containing `bin` directory.
6. Build the project (Ctrl+B), then use **Build > Deploy** to install

> **Note:** The **Always deploy before running** checkbox in **Edit > Preferences > Build & Run**
> is enabled by default. If this is unchecked, running the project will not automatically build
> and deploy — you will have to manually build and deploy before each run.

### Building a Windows Installer

DsQt projects include an Inno Setup-based installer system. After building your project:

1. Install [Inno Setup 6](https://jrsoftware.org/isdl.php)
2. Build the installer:
    ```
    cd install
    make_installer.bat                # Production installer
    make_installer.bat --non-production # Development (NPI) installer
    make_installer.bat -npi             # Development (NPI) installer (short form)
    ```
3. Output is in `install/build/`

For full details on adding installer support to an existing project, customizing flags,
and DSAppHost configuration, see the [Installer Guide](Docs/articles/installer_guide.md).

## Adding Waffles to an Existing DsQt Project

Integrate Waffles directly into the existing application. Check its Qt/DsQt
compatibility, add missing module links and QML plugin imports, and connect the
stage to its existing content and application services. Merge the required
settings and assets into its deployment, and verify its existing workflows
alongside the added features.

Follow the [Waffles implementation guide](Docs/waffles-implementation-guide.md)
for the full integration steps. AssetViewer and ECPresenter provide reference
patterns for both existing projects and new projects created with ProjectCloner.

## Getting to know DsQt

For detailed documentation, see the articles in [Docs/articles/](Docs/articles/):

1. [Settings Guide](Docs/articles/settings_guide.md) - Configuration and settings system
2. [Logging Guide](Docs/articles/logging_guide.md) - Log file configuration and rotation
3. [Best Practices](Docs/articles/best_practices.md) - Recommended patterns and approaches
4. [SVG Color Reference](Docs/articles/svg_color_reference.md) - Working with SVG colors
5. [Installer Guide](Docs/articles/installer_guide.md) - Building Windows installers with Inno Setup
6. [TouchFilter Guide](Docs/articles/touch_filter.md) - Touch event filtering, jitter smoothing, transient suppression, and debug overlay
7. [Waffles Overview](Docs/waffles-overview.md) - Components, capabilities, and the DsQt/application boundary
8. [Waffles Implementation Guide](Docs/waffles-implementation-guide.md) - Build, content routing, presentations, quick menu, whiteboard, settings, and validation
9. [Waffles Whiteboard](Docs/articles/whiteboard.md) - Drawing API, controls, lifecycle, and PNG export
10. [Q-Sys Integration](Docs/articles/qsys_usage.md) - Engine settings, QML/C++ updates, and commands through the Elevate plugin

## Generating documentation

DsQt is documented using the doxygen syntax. Need to add doxygen to CmakeFiles.

### Linux + Mac

????
