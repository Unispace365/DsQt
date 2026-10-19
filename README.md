# DsQt

## Description (the what and the why)

DsQt (Dee Ess Cute) is a framework for interactive applications built on top of the [Qt framework](https://qt.io/)

DsQt is a fully featured windows framework for rapid production of multimedia applications.
It is a replacement for DS Cinder. It Relies on Qt and QML for most features  (Multimedia, Fonts, 3D rendering)
but adds comprensive configuration data modeling and integration with our
CMS backend for rapid prototyping and developemnt.

## Documentation

Use the [documentation index](Docs/README.md) to find setup, Waffles, Bridge,
settings, deployment, and module guides.

The generated Doxygen reference is published here:
<https://unispace365.github.io/DsQt/>

For Waffles applications, start with the [Waffles overview](Docs/waffles-overview.md)
and [consolidated implementation guide](Docs/waffles-implementation-guide.md).
The [Source Tour](Docs/waffles-tour/index.html) covers individual components;
open its HTML locally for the styled walkthrough.

- [DsMultiEffect: alpha masking, edge fades, rounded corners, and adding effects](Docs/articles/multieffect.md)

## Getting Started

The recommended application setup is **ProjectCloner + a published DsQt vcpkg
package from the registry**. vcpkg fetches the selected DsQt source and builds it
against your Qt kit, or restores compatible cached binaries. Application
developers do not need to build/install the DsQt library separately.

For an existing application, use the
[package and version switching guide](Docs/articles/switching_dsqt_packages.md).
It explains the changes to `CMakeLists.txt`, presets, manifests, and package pins.
The [vcpkg guide](Docs/vcpkg.md) covers local packages and publication.

### Windows prerequisites

- An x64 MSVC C++ toolchain compatible with your Qt kit, including the Windows SDK.
  Run Ninja builds in its x64 developer shell.
- A Qt MSVC x64 SDK meeting the selected package's requirements. The published
  `0.3.0-dev.224.af-waffle-glass` package needs **Qt 6.12.0** or newer compatible
  SDK, including Canvas Painter. Install the Qt components shown by ProjectCloner
  for your selected modules; add PDF, WebEngine, or Virtual Keyboard when your
  application uses those features. Qt itself is not installed by this DsQt port.
- CMake 3.29 or newer, Ninja, Git, and Git LFS.
- A bootstrapped vcpkg checkout. Set `VCPKG_ROOT` in your build environment to its
  root, for example `C:/vcpkg`.
- Git credentials with access to both private repositories:
  [DsQt](https://github.com/Unispace365/DsQt) and
  [vcpkg-registry](https://github.com/Unispace365/vcpkg-registry).

If vcpkg is not installed, clone [Microsoft/vcpkg](https://github.com/microsoft/vcpkg)
into your chosen directory and run `bootstrap-vcpkg.bat`. Preserve your project's
checked-in dependency baselines. Updating the vcpkg executable does not update
those pins, and moving all dependencies to the latest baseline is not a general
fix for build errors.

### Build and Run ProjectCloner

Use a current ProjectCloner. To build the tool from this checkout without
building the DsQt library, run the following in an x64 MSVC developer shell.
Substitute your installed Qt kit and repository paths:

```powershell
Set-Location C:/dev/DsQt
cmake -S Tools/ProjectCloner -B Tools/ProjectCloner/build/setup -G "Ninja Multi-Config" -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64 -DCMAKE_INSTALL_PREFIX=C:/dev/DsQt/Tools/ProjectCloner/build/setup/install
cmake --build Tools/ProjectCloner/build/setup --config Release
cmake --install Tools/ProjectCloner/build/setup --config Release
& C:/dev/DsQt/Tools/ProjectCloner/build/setup/install/bin/appProjectCloner.exe
```

Keep the checkout available: the standalone tool can use its compiled-in
`Examples/ClonerSource` path. Run the installed tool so its deployed Qt DLLs,
plugins, and QML imports are available. The generated registry application
fetches DsQt independently of this checkout.

If you also develop the library, `Library/build_and_install.bat -qt 6.12.0 -tools`
is the combined library/tool build alternative. `-vcpkg` is a separate local
source-package workflow; see [the package guide](Docs/vcpkg.md).

## Creating a New Project

Always create a new DsQt application with **ProjectCloner**. AssetViewer and
ECPresenter are reference implementations. For adding Waffles to an existing
application, use the [existing-project path](#adding-waffles-to-an-existing-dsqt-project).

### Using ProjectCloner

1. [Build and run ProjectCloner](#build-and-run-projectcloner).
2. Select **Published vcpkg package**, the normal default, then **Refresh**.
   An explicit `DSQT_LOCAL_PACKAGE_ROOT` environment setting instead starts the
   tool in local-package mode. Older installed tool versions may require selecting
   the published source manually.
3. Choose a compatible published version. Stable packages appear first;
   development builds require explicit selection. For this Waffles branch,
   `0.3.0-dev.224.af-waffle-glass` is a published example. Older packages without
   supported module metadata are shown as unavailable for this template.
4. Choose the modules the application needs. Core is required; Waffles, Bridge,
   Qsys, Spout, Touch, and TouchEngine are optional. Add Bridge for CMS content.
5. Review the Qt requirements and select a compatible installed kit. Choose the
   destination folder and application name, then click **Clone**.

ProjectCloner creates the application's CMake files, QML/C++ startup, settings,
and module-specific assets. It also writes `dsqt-project.json`, matching vcpkg
features, an exact DsQt version override, and the registry commit containing it.
Existing projects keep those pins until deliberately updated. The ungenerated
ClonerSource template is not itself a configured registry application.

### Building Your New Project

From the generated application directory, in a matching x64 MSVC developer shell:

```powershell
$env:VCPKG_ROOT = 'C:/vcpkg'
cmake --list-presets
cmake --list-presets=build
cmake --preset ninja-vcpkg-6.12.0
cmake --build --preset ninja-vcpkg-6.12.0-release
cmake --install build/ninja-vcpkg-6.12.0 --config Release
```

These are example names for a generated Ninja/Qt 6.12.0 registry selection;
use the names actually listed in your project. Run the application from
`build/ninja-vcpkg-6.12.0/DEPLOY/Release/bin/`. Use the `-debug` build preset and
`--config Debug` for a Debug deployment.

For **Qt Creator**, open the generated `CMakeLists.txt`, select the corresponding
preset and Qt kit, and configure. Add a **CMake install** deploy step. Set the run
executable and working directory to the installed application's `bin` directory,
and deploy after building. Ensure the configure environment contains `VCPKG_ROOT`
and can authenticate to the private Git repositories.

For a different package, Qt kit, or source mode, follow
[Switching DsQt packages, versions, and Qt kits](Docs/articles/switching_dsqt_packages.md).
For local path overrides, see [CMake user presets](Docs/articles/cmake_user_presets.md).

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
11. [TouchEngine Rewrite and Multi-Backend Stabilization](Docs/articles/touchengine_rewrite_and_backend_stabilization.html) - Architecture, migration, diagnostics, testing, and graphics-backend stabilization

## Generating documentation

DsQt is documented using the doxygen syntax. Need to add doxygen to CmakeFiles.

### Linux + Mac

????
