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

### Choose a package and modules in ProjectCloner

Use **Published vcpkg package**, the normal default. An explicit
`DSQT_LOCAL_PACKAGE_ROOT` environment setting starts ProjectCloner in local mode;
installed-library and local-package sources are also available in the selector.
Use **Refresh** to read published versions with your existing
Git credentials. The cloner shows the package's minimum Qt version and enables
compatible installed MSVC x64 kits. Local and installed binaries require their
exact build kit. Registry builds use the kit selected for this project.

Core is required. Bridge, Waffles, Qsys, Spout, Touch, and TouchEngine are
independent choices with their required modules included automatically. Start
with Core and enable what the application needs. Waffles can run without Bridge;
add Bridge when the application needs CMS content and Bridge Sync.

This preview's Core uses Canvas Painter and requires Qt 6.12, even without
Waffles. Packages without verified module metadata are labelled unavailable for
this template rather than assigned a guessed Qt requirement.

The generated `dsqt-project.json` records the choice. ProjectCloner also writes
matching vcpkg features, exact published-version and registry-commit pins, and
CMake presets. The generated project links and registers only those modules.
Waffles content mappings, Waffles artwork, and its starter view are omitted
when Waffles is off. Bridge initialization/settings and the TouchDesigner
example are conditional on their modules. Configure your own Bridge endpoint
and schema mappings before using live content.

To change modules after generation, update `dsqt-project.json` and the matching
`dsqt-local` / `dsqt-registry` dependency feature lists in `vcpkg.json` together;
restore the module-specific starter files if needed. Regenerating into a new
folder is the easiest way to compare the necessary changes.

### Prerequisites

* Generate new applications with **ProjectCloner**. The ClonerSource directory is its template.
* For local package or installation builds, get the [DsQt Waffles branch](https://github.com/Unispace365/DsQt/tree/af/waffle-glass). Published-package consumers do not need a separate source checkout.
* Install a compatible Qt kit with the components required by your selected modules, a matching x64 MSVC compiler, CMake, Ninja, and vcpkg. This preview requires Qt 6.12; installed and local binary packages require their exact Qt version.
* Select the registry package, local package, or ordinary installation workflow below.
* If Bridge is selected, configure [Bridge Sync](https://github.com/Unispace365/bridge-sync) and enable it in `settings/engine.toml` when the connection is ready.

### Building with the published DsQt vcpkg package

Generate the application with ProjectCloner and select a published package
with verified module metadata. It writes the exact version override and
registry commit needed by this mode. The ungenerated template still pins a
registry commit containing the old `0.2.0.4` package; it is not a configured
application. The manifest leaves version selection to ProjectCloner instead
of naming a synthetic minimum version that does not exist in the registry.

Development releases use `<base>-dev.<Build Library run number>.<branch-slug>`,
for example `0.3.0-dev.228.af-waffle-glass`. Main releases use
`<base>.<Build Library run number>`, for example `0.3.0.241`. Both use vcpkg's
`version` field. The number is automatic; source manifests keep the base version.
The package, release tag, and ZIP share the same generated version.

Registry publication is a separate manual step after the release build succeeds.
Only main releases change the registry's default DsQt version. Use ProjectCloner's
**Refresh** to see published versions: stable releases appear first, followed by
development builds. Choose development builds explicitly; the generated project
pins that exact version and registry commit. Updating the registry commit alone
does not select a development package. Merging a branch does not change an
existing application's package selection.

The development package `0.3.0-dev.224.af-waffle-glass` is published at registry
commit `40c745a4552ece07cea659933844c858a9d88d59`; it requires Qt 6.12.0 or newer
compatible SDK. Select it explicitly in ProjectCloner. In the generated project,
use a matching x64 MSVC developer prompt:

```bat
set "VCPKG_ROOT=C:\vcpkg"
cmake --preset ninja-vcpkg-6.12.0
cmake --build --preset ninja-vcpkg-6.12.0-release
cmake --install build/ninja-vcpkg-6.12.0 --config Release
```

No local DsQt checkout or `DSQT_LOCAL_PACKAGE_ROOT` is needed. Git credentials
must allow access to both private repositories, and Git LFS must be installed.
vcpkg fetches the published source commit and builds it against the selected Qt
kit, or restores compatible binaries from its cache. Qt itself must already be
installed. Use `-DDSQT_REGISTRY_QT_ROOT=D:/Qt` when Qt lives outside `C:/Qt`.

ProjectCloner generates registry presets for selected Qt versions >= 6.12.
The preset generates a matching triplet in its build directory and installs
DsQt under that application's `vcpkg_installed` directory. Deployment is under
`build/ninja-vcpkg-6.12.0/DEPLOY/Release/bin/` (or `Debug/bin/`).

### Changing packages, versions, Qt kits, or modules

Follow [Switching DsQt packages, versions, and Qt kits](https://github.com/Unispace365/DsQt/blob/af/waffle-glass/Docs/articles/switching_dsqt_packages.md).
A published-version change updates the DsQt override and registry commit together.
A Qt change updates the kit settings; a source-mode change selects a separate build
preset. `dsqt-project.json` records the selection, but changing its `source` or
`version` alone does not change the build. Preserve your application code when
adopting newer scaffolding. Only presets for the originally selected source are
generated; the guide shows how to add the other modes to an existing application.

### Building with a local DsQt vcpkg package

First, set `VCPKG_ROOT` to a bootstrapped vcpkg checkout and build the package:

```bat
set "VCPKG_ROOT=C:\vcpkg"
C:\dev\DsQt\Library\build_and_install.bat -vcpkg -qt 6.12.0
```

Replace the vcpkg/checkout paths and Qt version for your machine. A successful
build publishes under `%LOCALAPPDATA%\DsQt\vcpkg` and updates its default.
ProjectCloner's **Local vcpkg package** choice discovers that package automatically
and records the store in the generated preset. A local preset
without an explicit path uses the same default store. Explicit CMake/preset
paths, then `DSQT_LOCAL_PACKAGE_ROOT` in the environment, override discovery.

Then, from this generated application's directory in a matching x64 MSVC developer prompt:

```bat
set "VCPKG_ROOT=C:\vcpkg"
cmake --preset ninja-vcpkg-local
cmake --build --preset ninja-vcpkg-local-release
cmake --install build/ninja-vcpkg-local --config Release
```

Run from `build/ninja-vcpkg-local/DEPLOY/Release/bin/`. Use the `-debug` build
preset and `--config Debug` for a Debug deployment. The package metadata selects
the exact Qt kit, and vcpkg installs the dependencies into this app's build tree.
The app and DsQt Library must use the same `builtin-baseline` in `vcpkg.json`.

The package and default binary cache are local to your machine. Multiple apps
can reuse that cache; no registry publication occurs. After changing DsQt,
repeat the package command, then configure fresh to adopt the latest default.
Explicit snapshot selections remain unchanged unless switched to the store.
Run from this app's directory:

```bat
cmake --fresh --preset ninja-vcpkg-local -DDSQT_LOCAL_PACKAGE_ROOT="%LOCALAPPDATA%\DsQt\vcpkg"
cmake --build --preset ninja-vcpkg-local-release
cmake --install build/ninja-vcpkg-local --config Release
```

Replace the preset/build directory names with those in your app. An exact
package directory instead of the store path selects a specific snapshot.
Each changed source snapshot has a distinct local package version. Failed builds
leave the previous default available.
Keep the package folder in place. See the
[DsQt vcpkg guide](https://github.com/Unispace365/DsQt/blob/af/waffle-glass/Docs/vcpkg.md)
for cache sharing, custom Qt paths, and adopting this in an existing project.

### Building with an ordinary DsQt installation

Build/install DsQt into `Documents/DsQt`, then select an installation-based
preset matching your kit. ProjectCloner generates Ninja Multi-Config presets
with separate Debug and Release build presets, for example:

```bat
cmake --preset ninja-6.12.0
cmake --build --preset ninja-6.12.0-release
cmake --install build/ninja-6.12.0 --config Release
```

Use `ninja-6.12.0-debug` to build Debug. Deployment always
uses the selected build directory's `DEPLOY/Release/bin/` or `DEPLOY/Debug/bin/`.

To see all available presets: `cmake --list-presets`

If your Qt is not installed under `C:\Qt\` or you need a different preset, create a
`CMakeUserPresets.json` — see the [CMake user presets guide](https://github.com/Unispace365/DsQt/blob/af/waffle-glass/Docs/articles/cmake_user_presets.md).

### Building (Qt Creator)

1. File > Open File or Project, navigate to this project's `CMakeLists.txt`
2. Select `ninja-vcpkg-6.12.0` for a published package, `ninja-vcpkg-local` with `VCPKG_ROOT` in
   the configure environment (the local package is discovered automatically), or an installation-based preset matching your kit
   (see `cmake --list-presets`), then click Configure Project
3. Go to **Projects** > **Deploy Settings** and click **Add Deploy Step** > **CMake install**
4. Go to **Projects** > **Run Settings** and set the **Executable** to
   `<build-directory>/DEPLOY/Release/bin/<your-exe>.exe` (or `Debug/bin/`) and the **Working directory** to the
   same directory as that executable
5. Build the project (Ctrl+B), then use **Build > Deploy** to install

> **Note:** The **Always deploy before running** checkbox in **Edit > Preferences > Build & Run**
> is enabled by default. If this is unchecked, running the project will not automatically build
> and deploy — you will have to manually build and deploy before each run.

### Hardware

* Single touch screen at 3840x1440

### Features

* Selected DsQt modules
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
