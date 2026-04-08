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