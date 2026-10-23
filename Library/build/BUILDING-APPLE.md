# Building DsQt for macOS and iOS

Use this guide to build and install the library. For an explanation of the
presets, toolchains, dependencies, and generated files, see
[How the Apple build works](BUILDING-APPLE-EXP.md).

## Requirements

- macOS with the full Xcode developer directory selected. The script rejects a
  Command Line Tools-only selection for all its targets.
- CMake 3.29 or newer on `PATH`.
- Ninja for macOS builds. The script also looks for `Tools/Ninja/ninja` under
  `~/Qt`, `/opt/Qt`, and `/Applications/Qt` when Ninja is not on `PATH`.
- A working vcpkg checkout. Set `VCPKG_ROOT`, or install it at `~/vcpkg`, which the
  script detects automatically.
- Qt 6.9 or newer for the selected target, including Multimedia. iOS builds
  need the base iOS kit and the matching macOS Qt tools. Touch uses Qt GUI
  private headers by default; see the fallback option below.

Qt 6.11.1 passed Debug and Release compilation and installation checks for all
four targets during the Apple build update. Those checks used temporary install
folders; they did not deploy an application to a device or simulator.

## Choose a target

Run these commands from the repository's `Library` directory:

```sh
./build_and_install.sh -qt 6.11.1
```

The interactive menu offers:

| Choice | Preset | Output target |
| --- | --- | --- |
| 1 | `macos` | macOS desktop, using the configured/native architecture |
| 2 | `ios-device` | Physical iOS devices, arm64 |
| 3 | `ios-simulator` | iOS simulator on Apple Silicon, arm64 |
| 4 | `ios-simulator-intel` | iOS simulator, x86_64 |

For a build without a prompt, specify one target explicitly:

```sh
./build_and_install.sh macos -qt 6.11.1
./build_and_install.sh ios-device -qt 6.11.1
./build_and_install.sh ios-simulator -qt 6.11.1
./build_and_install.sh ios-simulator-intel -qt 6.11.1
```

Each invocation builds and installs both **Debug and Release** for its selected
target. If the target is omitted in a noninteractive session, it defaults to
macOS. No connected device or running simulator is needed to build the library.

## Select a complete Qt installation

A version argument searches `~/Qt`, `/opt/Qt`, then `/Applications/Qt`, using the
`macos` subdirectory for macOS and `ios` for either simulator or device builds.
You can also provide a kit directory directly:

```sh
./build_and_install.sh macos -qt "$HOME/Qt/6.11.1/macos"
./build_and_install.sh ios-device -qt "$HOME/Qt/6.11.1/ios"
```

For iOS, the environment variable is another option:

```sh
export QT_IOS_ROOT="$HOME/Qt/6.11.1/ios"
./build_and_install.sh ios-simulator
```

`-qt` takes precedence over `QT_IOS_ROOT`. Supply the target kit directory, not
the version's parent directory. The script does not install Qt or automatically
fall back to a different version.

### Error: `qt.toolchain.cmake not found under QT_IOS_ROOT`

The selected folder is missing the base Qt iOS build support, or the path points
to the wrong directory. A folder named `ios` can exist with only add-ons: during
troubleshooting, the local Qt 6.11.2 iOS folder contained PDF add-ons but no base
iOS kit. This is an installation issue, not a restriction on Qt 6.11.2.

Check the selected installation:

```sh
ls "$HOME/Qt/6.11.2/ios/lib/cmake/Qt6/qt.toolchain.cmake"
ls "$HOME/Qt/6.11.2/ios/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake"
```

Use a complete version such as the verified 6.11.1 installation, or add the
missing base iOS kit and iOS Multimedia module to the desired version. Keep the
matching macOS Qt tools installed as well. Do not copy a toolchain file from a
different Qt version; it describes that version's installation.

```sh
./build_and_install.sh ios-simulator-intel -qt 6.11.1
```

If this build folder was previously configured with another Qt version, add
`-hard-rebuild` to remove the old configuration before building.

## Build and install locations

| Preset | Build directory, relative to `Library` | Default install prefix |
| --- | --- | --- |
| `macos` | `build/macos` | `~/Documents/DsQt` |
| `ios-device` | `build/ios-device` | `~/Documents/DsQt-ios-device` |
| `ios-simulator` | `build/ios-simulator` | `~/Documents/DsQt-ios-simulator` |
| `ios-simulator-intel` | `build/ios-simulator-intel` | `~/Documents/DsQt-ios-simulator-intel` |

Debug and Release archives are installed under `lib/Debug` and `lib/Release`.
Headers, QML files, and CMake package files share the selected prefix. Choose the
matching installation when building an application: device arm64 and simulator
arm64 are different platforms. The script does not package an XCFramework.

## Common options

```sh
# Build without installing.
./build_and_install.sh ios-device -qt 6.11.1 -no-install

# Reuse a previously configured build folder.
./build_and_install.sh macos -qt 6.11.1 -no-configure

# Delete this target's build folder, configure, build, and install again.
./build_and_install.sh ios-simulator -qt 6.11.1 -hard-rebuild

# Build and run desktop unit tests after the Debug build.
./build_and_install.sh macos -qt 6.11.1 -test

# Build Touch using its public Qt event-delivery fallback.
./build_and_install.sh ios-device -qt 6.11.1 -no-private-reinject

./build_and_install.sh --help
```

Use a fresh build folder when changing Qt versions, generators, or target
architectures. This includes upgrading an old Intel `build/ios-simulator` folder
to the current Apple Silicon preset. `-hard-rebuild` deletes only the selected
build folder; it does not remove an existing installation. Do not combine it
with `-no-configure`.

The iOS presets disable library signing by default. `-team TEAM_ID` enables
signing and sets the development team if your workflow needs it. Application
signing and deployment are separate steps.

`-test` and `-tools` are macOS-only. Test failures currently print a warning and
allow the script to continue; inspect the test output or JUnit report rather
than relying only on the script's final exit status.

On iOS, Bridge can query and watch its SQLite database, but it cannot launch the
desktop BridgeSync subprocess. Requesting that launch logs a warning. TouchEngine
and Spout are Windows-only modules and are excluded from these Apple builds.
