# How the Apple build works

This guide explains the implementation behind the commands in
[Building DsQt for macOS and iOS](BUILDING-APPLE.md). Paths below are relative to
`Library` unless another base is stated.

## From a command to installed libraries

The shell script coordinates the tools. CMake decides what must be compiled,
Ninja or Xcode executes the build, and CMake copies the results into an install
prefix that another project can consume.

```text
build_and_install.sh: choose target and resolve dependencies
    |
    v
CMake configure preset: select SDK, architecture, generator, and output folder
    |
    v
vcpkg toolchain -> Qt iOS toolchain (for iOS targets)
    |
    v
CMakeLists.txt: define library modules, QML generation, and install rules
    |
    v
Debug build -> optional desktop tests -> Release build
    |
    v
Debug install -> Release install -> optional desktop tools
```

A successful library build does not launch an application. Device deployment,
simulator execution, application signing, and packaging an XCFramework belong to
later application or distribution workflows.

## The files that control the build

| File | Responsibility |
| --- | --- |
| `build_and_install.sh` | Parse arguments, show the menu, check tools, locate Qt, update vcpkg, and run configure/build/install commands. |
| `CMakePresets.json` | Define named platform configurations, generators, architectures, dependency triplets, and default output paths. |
| `CMakeLists.txt` | Define the library modules and shared installation rules. |
| `DsQt/*/CMakeLists.txt` | Define each module's sources, Qt dependencies, QML module, and header installation. |
| `vcpkg.json` | Declare third-party dependencies and the dependency baseline. |
| `cmake/triplets/arm64-ios-simulator.cmake` | Tell vcpkg to build arm64 dependencies for the simulator SDK. |
| `cmake/DsqtConfig.cmake.in` | Generate the installed CMake package used by `find_package(Dsqt)`. |
| `VERSION.txt` | Supply the DsQt project/package version. |

The Windows `ninja` preset contains MSVC settings. It is separate from the Apple
`macos` preset even though both use Ninja Multi-Config.

## A target needs both a platform and an architecture

An architecture describes the CPU instructions. A platform selects the operating
system SDK and binary environment. The same CPU architecture can appear in
incompatible builds: arm64 macOS, arm64 iOS device, and arm64 iOS simulator
libraries are distinct outputs.

| Preset | Generator | SDK selection | Architecture | vcpkg target triplet |
| --- | --- | --- | --- | --- |
| `macos` | Ninja Multi-Config | macOS SDK selected by the toolchain | Native/configured; not explicitly fixed by this preset | Automatically selected by vcpkg |
| `ios-device` | Xcode | `iphoneos` | `arm64` | `arm64-ios` |
| `ios-simulator` | Xcode | `iphonesimulator` | `arm64` | `arm64-ios-simulator` |
| `ios-simulator-intel` | Xcode | `iphonesimulator` | `x86_64` | `x64-ios` |

`CMAKE_OSX_SYSROOT` chooses the Apple SDK and `CMAKE_OSX_ARCHITECTURES` specifies
the iOS target architecture. The iOS presets set `CMAKE_SYSTEM_NAME=iOS` so CMake
configures a cross build. Each preset uses its own build and install directories
to avoid mixing these outputs.

The Intel simulator preset inherits the Apple Silicon simulator settings, then
overrides the architecture, dependency triplet, build directory, and install
prefix. It still uses the simulator SDK.

## How Qt is located

For `-qt 6.11.1`, the script searches the following roots in order:

1. `~/Qt`
2. `/opt/Qt`
3. `/Applications/Qt`

It looks for `<root>/6.11.1/macos` when building macOS and
`<root>/6.11.1/ios` for either kind of iOS build. A path supplied with `-qt` is
used directly. Explicit paths are normalized before CMake uses them.

For macOS, the resolved path is passed as `CMAKE_PREFIX_PATH` and `Qt6_DIR`.
For iOS, the script exports `QT_IOS_ROOT`; the preset uses it to locate
`lib/cmake/Qt6/qt.toolchain.cmake`. If `-qt` is absent, iOS builds can use an
existing `QT_IOS_ROOT`. Without `-qt` on macOS, Qt discovery is left to CMake's
existing configuration and search paths; the script does not select the newest
installed version.

The host and target Qt installations serve different purposes. Host tools run
on the Mac to generate C++, resources, and QML metadata. Target libraries are
compiled for the selected Apple platform. Keeping matching macOS and iOS Qt
versions provides the tools and libraries expected by the Qt iOS configuration.

A directory's existence does not prove it is a complete Qt kit. An add-on such as
Qt PDF can create an `ios` folder without installing the base iOS libraries or
toolchain file. The script first resolves the directory and then checks the iOS
toolchain file. CMake subsequently checks required modules such as Multimedia.
It does not silently substitute another Qt version if either check fails.

The quick guide documents the missing-toolchain error observed with an incomplete
local Qt 6.11.2 installation and how to select a complete installation instead.

## Why two toolchains are involved

The top-level CMake toolchain is vcpkg's
`$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`. It integrates dependency discovery
and manifest installation with the project's configure step.

For iOS, `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` points to Qt's iOS toolchain. This lets
vcpkg load Qt's platform configuration while retaining vcpkg's package handling.
Qt supplies its installation paths and Apple cross-compilation settings; the
preset supplies the selected device/simulator SDK and architecture. Deployment
target defaults come from the Qt/toolchain configuration unless overridden;
these presets do not specify a fixed minimum iOS version.

The build does not ask vcpkg to install Qt. Qt is a separate installation. On
Apple platforms, the manifest's direct third-party dependency is `tomlplusplus`.
The `vulkan` and `spout2` dependencies are conditional on Windows and are omitted.

A vcpkg **triplet** describes how to build dependencies, including their target
architecture, platform, and linkage. The custom simulator triplet contains:

```cmake
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME iOS)
set(VCPKG_OSX_SYSROOT iphonesimulator)
```

The simulator sysroot and distinct triplet name keep arm64 simulator dependencies
separate from arm64 device dependencies. Selecting arm64 alone would not express
that distinction. `VCPKG_OVERLAY_TRIPLETS` points vcpkg at this repository's
`cmake/triplets` directory.

## What happens on each script invocation

1. **Read arguments and select a target.** With a terminal and no explicit
   target, the menu appears. Enter chooses macOS. Without a terminal, an omitted
   target defaults to macOS. Only one preset is accepted per invocation.
2. **Check the environment.** The script requires macOS, CMake, and a selected
   full Xcode installation. For macOS builds it also checks Ninja, including the
   copies commonly installed under Qt's `Tools/Ninja` directory. It rejects
   `-test` and `-tools` for iOS.
3. **Prepare vcpkg.** It uses `VCPKG_ROOT`, falling back to `~/vcpkg`. For a Git
   checkout, it attempts a fetch and fast-forward pull. A failed update produces
   a warning and the script continues with available sources. A changed checkout
   or newer `vcpkg-tools.json` triggers bootstrapping of the vcpkg executable.
4. **Resolve Qt and handle cleaning.** The script validates Qt before processing
   clean/rebuild flags. Even a clean-only invocation therefore performs the
   earlier environment checks and vcpkg preparation.
5. **Configure.** `cmake --preset <target>` reads the selected preset, prepares
   dependencies, finds Qt modules, and generates the build system. Optional
   arguments enable tests, tools, alternate touch delivery, or a signing team.
6. **Build Debug, optionally run tests, then build Release.** Each configuration
   uses its corresponding build preset. Incremental builds reuse unchanged
   outputs. Tests run only when `-test` is supplied.
7. **Install both configurations.** Unless `-no-install` is given, CMake installs
   Debug and Release even when compilation found nothing to rebuild. This allows
   missing installed files to be restored. Optional desktop tool installation
   follows. A timing summary ends the run.

Most configure/build/install failures stop the script. There are deliberate
exceptions in the current implementation: vcpkg update failures warn, clean
commands ignore failure, and failing unit tests warn while the build continues.
The test report is written under `build/<target>/test-results.xml`.

The script's overall timer starts after dependency preparation and cleaning. Its
reported installation time covers the library install steps, while separate
ProjectCloner work is included in the later overall elapsed time.

## Configurations, caches, and command-line options

Both Apple generators support multiple configurations in one build directory.
`Debug`, `Release`, and `RelWithDebInfo` presets exist, but the script builds and
installs only Debug and Release. This is why the commands use `--config` or build
presets rather than changing `CMAKE_BUILD_TYPE` for every library build.

| Option | Effect |
| --- | --- |
| `-no-configure` | Skip the script's explicit configure step and reuse the existing build. The underlying build system can still regenerate CMake files when needed. |
| `-no-install` | Skip library installation and the later desktop tools installation steps. |
| `-clean` | Request clean targets for Debug and Release, then exit; keep the CMake cache. |
| `-hard-clean` | Delete `build/<target>`, then exit. |
| `-rebuild` | Request clean targets, then continue through the normal build flow. |
| `-hard-rebuild` | Delete the selected build directory, then continue through the normal configure/build/install flow. |
| `-test` | Enable desktop test targets and run CTest after Debug. |
| `-tools` | Enable desktop tools in CMake and perform the script's ProjectCloner/template installation steps. |
| `-no-private-reinject` | Set `DSQT_TOUCH_NO_PRIVATE_REINJECT=ON`. |
| `-team ID` | Set the Xcode development team and enable code signing. |

The long `--` forms of these options are accepted too. Use one clean/rebuild mode
at a time. `-hard-rebuild -no-configure` deletes the very configuration the next
step would need, so use `-hard-rebuild` without `-no-configure`.

CMake caches the Qt location, toolchain, generator, architecture, and feature
settings. Reconfigure when changing options, and use a fresh build directory
when switching Qt kits, generators, or architectures. In particular, an old
Intel `ios-simulator` build folder must be recreated for the current arm64 preset.
Deleting build output does not uninstall files already copied to an install
prefix and does not clear the global vcpkg download/binary caches.

Some flags only set cached options to `ON`; omitting them on a later run does
not necessarily turn those options off. For example, the script does not pass
an explicit `OFF` to reverse an earlier `-no-private-reinject`. A fresh build
restores the project's defaults, or the relevant cache variable can be changed
through CMake. With `-no-configure`, newly supplied configure options are not
applied by the script.

## What the installation contains

The quick guide lists the four default prefixes. Within each prefix, the library
installation has this general shape:

```text
<prefix>/
    include/                    Public module headers
    lib/Debug/                  Debug module and QML plugin archives
    lib/Release/                Release module and QML plugin archives
    lib/cmake/Dsqt/              DsqtConfig.cmake and version file
    qml/Dsqt/<module>/           qmldir, qmltypes, and QML source files
```

CMake installs the Core, Bridge, Waffles, and Touch libraries and their QML
plugins. The package template defines imported targets with configuration-specific
archive locations. It creates them manually because the Qt QML modules include
internal object-library dependencies that this project does not export directly.

The installed package uses `find_dependency` to find Qt and `tomlplusplus`, and
sets the DsQt QML import path. Installing DsQt does not bundle every Qt or vcpkg
dependency into a self-contained SDK. A consuming application still needs the
appropriate Qt kit and dependency configuration.

Apple installation also writes an entry under `~/.cmake/packages/Dsqt`. When
multiple platform installations exist, explicitly point a consuming CMake project
at the matching package directory, for example by setting:

```sh
-DDsqt_DIR="$HOME/Documents/DsQt-ios-simulator/lib/cmake/Dsqt"
```

That is an argument for the application's CMake configure command, not an option
to this build script. The application's SDK, architecture, Qt kit, and dependency
triplet must also match. In its `CMakeLists.txt`, a consumer can discover the
package with `find_package(Dsqt CONFIG REQUIRED)` and link `Dsqt::Dsqt`, or select
individual modules. Application QML plugin import and deployment setup remains
part of the application build.

For `-tools`, the script additionally builds ProjectCloner in the repository's
`Tools/ProjectCloner/build/macos` directory, installs it under
`~/Documents/DsQt/Tools/ProjectCloner`, and refreshes the template at
`~/Documents/DsQt/Tools/ClonerSource`. The library CMake rules also include tools
and an `Examples/ClonerSource` install when `DSQT_BUILD_TOOLS` is enabled.

## Platform-specific behavior

**TouchEngine and Spout:** These modules are Windows-only in this repository.
Apple builds omit them. The umbrella library links TouchEngine only when its
target exists, and its QML metadata is installed only when the module is built.
This prevents Apple configuration and installation from referring to absent
Windows module outputs.

**BridgeSync:** Qt disables subprocess support on iOS. Bridge checks
`QT_CONFIG(process)` around its process member, lifecycle guard, and launch/stop
code. On platforms without that feature, launching BridgeSync returns failure
and logs a warning if launch was requested. Database reading and watching remain
available; this change does not provide an iOS replacement sync process.

**Touch event delivery:** The default Touch configuration uses `Qt6::GuiPrivate`
to re-inject filtered touches through Qt's input pipeline. The fallback flag
uses `QCoreApplication::sendEvent` and removes that private-header requirement
for Touch. These paths have different behavior; the flag is a compatibility
choice when the private API is unavailable, not a way to supply a missing Qt kit.

**Signing:** The iOS presets set
`CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO`. The script's `-team` option
changes that to `YES` and supplies `CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM`.
Building these static libraries normally does not require an attached device or
a provisioning profile. Signing a finished iOS application is handled separately.

## Diagnosing a failure by stage

| Failure | First thing to check |
| --- | --- |
| `cmake not found` or Ninja missing | Tool installation and `PATH`; the script only auto-detects Ninja in the listed Qt roots. |
| Only Command Line Tools selected | `xcode-select -print-path`; select the full Xcode developer directory. |
| `VCPKG_ROOT` or vcpkg executable missing | The checkout path and whether vcpkg has been bootstrapped. |
| Missing `qt.toolchain.cmake` | Correct target-kit directory and a complete base Qt iOS installation. |
| Required `Qt6Multimedia` missing | Multimedia installed for the selected Qt version and target. |
| Compiler, architecture, or SDK mismatch after a change | Old CMake cache; recreate only the affected target's build directory. |
| Required `GuiPrivate` missing | Matching private GUI headers, or the documented Touch fallback. |
| Install reports a missing archive | Whether that configuration completed compilation before installation. |
| Application links against the wrong platform | Its SDK/architecture, dependency triplet, and `Dsqt_DIR`. |

Read the first substantive error in the configure or compiler output. Later
messages such as “Configuration failed” or “Debug build failed” summarize that
failure rather than identify a new cause.
