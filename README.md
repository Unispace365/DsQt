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

## Getting Started

> This version of DsQt has been built and is targeted at Windows. We hope to have Linux and Apple builds.

### Windows

#### Install Visual Studio 2022 - Community Edition

1. Download here: <https://www.visualstudio.com/>
2. Install it!
        - Make sure to select C++ development and ensure you have the latest windows SDK and visual studio updates!

#### Install Qt 6.10

1. Create a Qt.io account if you don't have one. <https://login.qt.io/register>
2. Go to <https://www.qt.io/download-qt-installer-oss> and download Qt for x64
3. Run the online installer.
4. You don't need to install everything. In fact I'd strongly recommend against it. But also it's hard to know what your going to need. I think the following are the important ones:

    * Extensions:
        * Qt PDF:
            * Qt 6.10.1:
                * MSVC 2022 x64
        * Qt WebEngine:
            * Qt 6.10.1:
                * MSVC 2022 x64
    * Qt:
        * Qt 6.10.1:
            * MSVC 2022 64-bit
            * Additional Libraries:
                * Qt Multimedia
                * Qt Image Formats
                * Qt Language Server
                * Qt Positioning
                * Qt Virtual Keyboard
                * Qt Quick 3d
                * Qt Quick Effect Maker
                * Qt WebChannel
                * (Everything else you can get just to be safe)
    * Qt Creator (check all boxes)
5. Depending on what you selected this can take a while to download (hours sometimes).

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
2. *(First time, optional)* If your Qt is **not** installed under `C:\Qt\`, copy
   `CMakeUserPresets.example.json` → `CMakeUserPresets.json` and edit the Qt path inside.
   If Qt is in `C:\Qt\` the build auto-discovers the latest version — no config needed.
3. Run `build_and_install.bat` — configures, builds Debug + Release, and installs to
   `%USERPROFILE%\Documents\DsQt`
4. To target a specific Qt version: `build_and_install.bat ninja-6.10.2`

**Alternative (Qt Creator):**

1. Open Qt Creator
2. File > Open File or Project, navigate to `DsQt/Library/CMakeLists.txt`
3. Qt Creator will detect the CMakePresets.json — select the `ninja` preset (or a
   version-pinned preset from your `CMakeUserPresets.json`)
4. Click Configure Project, then Build > Install

#### Build and Run ProjectCloner

**Preferred (command-line):**

1. `cd Library`
2. Run `build_and_install.bat` with the `DSQT_BUILD_TOOLS` option enabled, or open
   `DsQt/Tools/ProjectCloner/CMakeLists.txt` directly
3. Install deploys to `%USERPROFILE%\Documents\DsQt`
4. Run: `%DS_QT_PLATFORM_100%\Tools\deploy\ProjectCloner\bin\appProjectCloner.exe`

**Alternative (Qt Creator):**

1. Open Qt Creator
2. File > Open File or Project, navigate to `DsQt/Tools/ProjectCloner/CMakeLists.txt`
3. Select the `ninja` preset (or a version-pinned preset) and click Configure Project
4. Build the project (Ctrl+B), then Build > Install
5. Run: `%DS_QT_PLATFORM_100%\Tools\deploy\ProjectCloner\bin\appProjectCloner.exe`

## Creating a New Project

### Using ProjectCloner (Recommended)

1. Run `appProjectCloner.exe` from `DsQt\Tools\deploy\ProjectCloner\bin\`
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

**Preferred (command-line):**

1. `cd <your-project>`
2. Run `build_and_install.bat` — auto-discovers Qt and builds Debug + Release
3. To target a specific Qt version: `build_and_install.bat ninja-6.10.2`
4. Run from: `build/ninja/DEPLOY/bin/`

**Alternative (Qt Creator):**

1. Open Qt Creator
2. File > Open File or Project, navigate to your project's `CMakeLists.txt`
3. Select the `ninja` preset (or a version-pinned preset) and click Configure Project
4. Build the project (Ctrl+B), then Build > Install
5. Run from: `build/ninja/DEPLOY/bin/`

### Building a Windows Installer

DsQt projects include an Inno Setup-based installer system. After building your project:

1. Install [Inno Setup 6](https://jrsoftware.org/isdl.php)
2. Build the installer:
    ```
    cd install
    make_installer.bat              # Development installer
    make_installer.bat --production # Production installer
    ```
3. Output is in `install/build/`

For full details on adding installer support to an existing project, customizing flags,
and DSAppHost configuration, see the [Installer Guide](Docs/articles/installer_guide.md).

## Getting to know DsQt

For detailed documentation, see the articles in [Docs/articles/](Docs/articles/):

1. [Settings Guide](Docs/articles/settings_guide.md) - Configuration and settings system
2. [Logging Guide](Docs/articles/logging_guide.md) - Log file configuration and rotation
3. [Best Practices](Docs/articles/best_practices.md) - Recommended patterns and approaches
4. [SVG Color Reference](Docs/articles/svg_color_reference.md) - Working with SVG colors
5. [Installer Guide](Docs/articles/installer_guide.md) - Building Windows installers with Inno Setup

## Generating documentation

DsQt is documented using the doxygen syntax. Need to add doxygen to CmakeFiles.

### Linux + Mac

????
