# Downstream | APP_NAME_

> "PROJECT_NAME_" for SuperClient.com 2024 Customer Experience Center in Anytown, OR

<img src="./docs/screenshot.png" height = "500px" />

---

* **Documentation**: [Confluence Link (Confluence)](https://downstream.atlassian.net/wiki/)
* **Jira Project**: [JIRA LINK](https://downstream.atlassian.net/jira/)
* **Developement CMS**: TBD
* **Production CMS**: TBD
* **Schema Repo**: [GitHub Link to Schema](https://github.com/Unispace365/)

## Compiling / Running

### Prerequisites

* Get [DsQt](https://github.com/Unispace365/DsQt) main branch
* Follow the [DsQt getting started guide](https://github.com/Unispace365/DsQt/) to install Visual Studio 2022, Qt, vcpkg, and build the DsQt Library
* Use [Bridge Sync](https://github.com/Unispace365/bridge-sync)

### Building (command-line)

1. Configure: `cmake --preset ninja-6.10.2`
2. Build: `cmake --build --preset ninja-6.10.2-debug` (or `-release`, `-relwithdebinfo`)
3. Install: `cmake --build --preset ninja-6.10.2-release --target install`
4. Run from: `build/ninja-6.10.2/DEPLOY/bin/`

To see all available presets: `cmake --list-presets`

If your Qt is not installed under `C:\Qt\` or you need a different preset, create a
`CMakeUserPresets.json` — see the DsQt repo's `Docs/articles/cmake_user_presets.md` for details.

### Building (Qt Creator)

1. File > Open File or Project, navigate to this project's `CMakeLists.txt`
2. Select the `ninja-6.10.2` preset (or a version-pinned preset from your
   `CMakeUserPresets.json`) and click Configure Project
3. Go to **Projects** > **Deploy Settings** and click **Add Deploy Step** > **CMake install**
4. Go to **Projects** > **Run Settings** and set the **Executable** to
   `build/ninja-6.10.2/DEPLOY/bin/<your-exe>.exe` and the **Working directory** to the
   same `DEPLOY/bin/` directory
5. Build the project (Ctrl+B), then use **Build > Deploy** to install

> **Note:** The **Always deploy before running** checkbox in **Edit > Preferences > Build & Run**
> is enabled by default. If this is unchecked, running the project will not automatically build
> and deploy — you will have to manually build and deploy before each run.

### Hardware

* Single touch screen at 3840x1440

### Features

* Waffles
* Custom layouts
* Ambient fun
* General kooky-ness

---

### People Involved

* **Lead Designer(s)** - Lucy McDesigner
* **Project Manager(s)** - Paris McManager
* **Original Developer(s)** - David McDeveloper, Kevin McCoder
* **Install Site(s):** Anytown, OR, USA
* **PM / Designer name:** ???
* **Github Name:** TBD
* **Name in project file:** APP_NAME_

[//]: # (This is a comment)

## macOS and iOS

Install Xcode, CMake 3.29+, a bootstrapped vcpkg checkout, and a complete Qt kit
including Multimedia and the modules required by DsQt. Build and install DsQt
for the same Qt version, SDK, and architecture as the app. The manifest supplies
compiled toml++ and glm; do not substitute a header-only toml++ shim.

| Configure preset | Target | Build presets |
| --- | --- | --- |
| `macos` | Native macOS | `macos-debug`, `macos-release` |
| `ios-device` | arm64 iOS device | `ios-device-debug`, `ios-device-release` |
| `ios-simulator` | arm64 iOS simulator | `ios-simulator-debug`, `ios-simulator-release` |
| `ios-simulator-intel` | x86_64 iOS simulator | `ios-simulator-intel-debug`, `ios-simulator-intel-release` |

All Apple presets use Xcode with Debug and Release in separate configuration
outputs within one build directory per target. Select a Qt kit whose FFmpeg
XCFrameworks contain the requested simulator architecture. Some older Qt kits
only include x86_64 simulator FFmpeg binaries.

Set `VCPKG_ROOT`, `QT_APPLE_ROOT` (the macos or ios Qt kit), and `DSQT_ROOT`
(the matching DsQt installation prefix). iOS also needs `QT_HOST_PATH` pointing
to the matching macOS Qt kit. No developer-specific paths belong in shared presets.

```sh
cmake --preset ios-simulator-intel
cmake --build --preset ios-simulator-intel-debug
cmake --build --preset ios-simulator-intel-release
```

For Qt Creator, copy `CMakeUserPresets.json.example` to `CMakeUserPresets.json`,
replace the example paths with your local installations, and use Build > Reload
CMake Presets. Select `local-macos` or `local-ios-simulator-intel`. Add local
presets inheriting `ios-device` or `ios-simulator` with matching DsQt paths as
needed. User presets are ignored by Git and excluded from newly cloned projects.
Use a fresh build directory when changing Qt versions, SDKs, or toolchains.
Empty variables expand into invalid paths, so fill in all required paths before
importing the presets. Local Debug and Release build presets are included.

For macOS distribution, run `cmake --install build/macos --config Release`;
the app and Qt deployment are placed under `build/macos/DEPLOY/Release`.
For iOS, select the matching simulator or device in Qt Creator/Xcode and deploy
the generated app bundle. Device deployment needs your bundle identifier,
Apple development team, and provisioning configured locally in Xcode or CMake.
The template does not embed a team ID or disable device signing.

Apple builds use Metal, include settings/data in the app bundle, and disable
source-tree QML hot reload in the bundled engine settings. iOS uses the native
keyboard and disables launching the desktop BridgeSync subprocess. Database
content must be supplied separately to the app's writable storage. Windows-only
TouchDesigner QML and installers are excluded. Qt's iOS helper links and embeds
FFmpeg from the selected Qt kit; macOS uses Qt's QML deployment helper.
