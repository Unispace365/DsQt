# Building on macOS

Install Xcode (select the full Xcode developer directory), CMake 3.29 or newer,
Ninja, vcpkg, and Qt 6.9 or newer. Qt must include Multimedia and the private
GUI headers used by Touch. iOS builds also need the matching desktop Qt tools.
Set `VCPKG_ROOT` to your vcpkg checkout; `~/vcpkg` is detected automatically.

From `Library`, run `./build_and_install.sh -qt 6.11.1` and choose a target.
For unattended builds, specify the target:

```sh
./build_and_install.sh macos -qt 6.11.1
./build_and_install.sh ios-device -qt 6.11.1
./build_and_install.sh ios-simulator -qt 6.11.1
./build_and_install.sh ios-simulator-intel -qt 6.11.1
```

Each command builds and installs both Debug and Release. `ios-simulator` targets
Apple Silicon (arm64); `ios-simulator-intel` targets x86_64. These are separate
SDK-specific library installations, not an XCFramework. No device or running
simulator is required to build the libraries. Library builds disable signing by
default; `-team TEAM_ID` enables signing with your development team.

Build folders are `Library/build/<target>`. Install locations are
`~/Documents/DsQt` for macOS and `~/Documents/DsQt-<target>` for iOS.
Use the matching installation when configuring your application.

`-qt` also accepts an absolute path to the `macos` or `ios` Qt kit directory.
For iOS, `QT_IOS_ROOT` can supply that path instead. In noninteractive sessions,
omitting the target keeps the macOS default. Use `--help` for additional options.

If upgrading an existing Intel `build/ios-simulator` folder to the Apple Silicon
preset, use `-hard-rebuild` once to remove its old configuration. Also use a fresh
build folder when switching Qt versions. `-hard-rebuild` deletes the selected
build folder. `-no-private-reinject` allows building Touch without private GUI
headers, with its documented fallback behavior.

On iOS, Bridge can query and watch its SQLite database, but it cannot launch the
desktop BridgeSync subprocess. Requesting that launch logs a warning.
