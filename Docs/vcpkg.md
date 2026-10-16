# DsQt source package for Waffles

The Waffles preview uses package name `dsqt` and
`"version-semver": "0.3.0-waffles.1"`. It is development code from
`af/waffle-glass`, not a main-branch release. `Library/VERSION.txt` keeps
the numeric CMake project version `0.3.0`; the port manifest carries the
prerelease suffix. Publishing must preserve that suffix and pin the exact
source commit. The port in this checkout is a template with a zero `REF`;
local validation substitutes its source before building.

This integration adapts the source-port work from `ph/develop`. The registry's
older `0.2.0.4` package does not contain the current Waffles implementation.
This preview is not published merely by building it locally.

## What a source port does

vcpkg fetches the pinned DsQt source, builds it against the consumer's Qt kit
and compiler, and stores the result in its binary cache. It does not install
Qt itself. Windows x64 with the MSVC Qt kit is currently supported.

The package contains Core, Bridge, Waffles, Qsys, Touch, TouchEngine, and Spout,
including their static QML plugins and module metadata. TouchEngine's SDK DLL
is included. The package triplet builds toml++ statically, so that dependency
does not add a DLL to deploy. Spout2 and Vulkan are vcpkg dependencies.

Prerequisites:

- Qt 6.12.0 or a newer compatible MSVC x64 kit. Install the modules required by
  Waffles, including Canvas Painter, Multimedia, ShaderTools, WebSockets,
  VirtualKeyboard, and PDF for PDF support.
- The matching x64 MSVC developer environment, CMake, Ninja, and vcpkg.
- Git access to the private DsQt repository and Git LFS for published sources.

WebEngine, PDF, and VirtualKeyboard availability is recorded in the installed
package. A consumer needs the same optional Qt modules that were present when
DsQt was built. The local port validation requires PDF explicitly.

## Local validation before publication

From the matching x64 MSVC developer shell, at the DsQt repository root:

```powershell
.\Library\vcpkg\Test-DsqtPort.ps1 -QtVersion 6.12.0 -VcpkgRoot C:\vcpkg
```

The default mode snapshots tracked and non-ignored `Library/` sources,
including uncommitted edits. A source digest is included in the overlay port
so that an edit invalidates the cached package. It does not publish anything.
It isolates staging, builds, and installed packages under
`Library/build/vcpkg-validation/port-test/`.

Additional modes:

```powershell
# Prepare an overlay and triplet without compiling.
.\Library\vcpkg\Test-DsqtPort.ps1 -QtVersion 6.12.0 -StageOnly

# Reinstall the isolated package and require restoration from the binary cache.
.\Library\vcpkg\Test-DsqtPort.ps1 -QtVersion 6.12.0 -CacheTest

# After committing and pushing, exercise the actual Git/LFS fetch.
.\Library\vcpkg\Test-DsqtPort.ps1 -QtVersion 6.12.0 -FromRemote -Commit <sha>
```

Remote validation deliberately bypasses cache reads so a cache hit cannot
conceal a broken fetch. Its commit must contain the packaging changes and be
available on GitHub. It cannot validate uncommitted code.

Use `-WorkRoot` to choose a different dedicated validation directory.
The helper only removes its own staged directories and its own installed test
package; it does not remove packages from the ordinary vcpkg installed tree.
Source overlays at different absolute paths can have different cache keys.

## Kit selection and cache reuse

The triplet records `DSQT_QT_VERSION`, `DSQT_QT_ROOT`, and `DSQT_QT_ARCH`.
Its content is part of vcpkg's ABI hash, together with the compiler, port, and
dependencies. Keep the triplet identical between the package build and its
consumer if cache reuse is desired. Different dependency baselines or compiler
versions can correctly cause another build.

Generate a triplet for an installed kit:

```powershell
.\Library\vcpkg\New-DsqtTriplet.ps1 -QtVersion 6.12.0 -OutputDirectory .\triplets
```

The committed `x64-windows-qt6-12-0.cmake` selects
`C:/Qt/6.12.0/msvc2022_64`. Custom installation paths require a matching generated
triplet. The installed CMake package rejects a consumer using another Qt
version or compiler family.

The package is a build recipe, while the binary cache contains compiled
packages. A team can configure `VCPKG_BINARY_SOURCES` to share compatible
builds. Building locally does not automatically upload to a team feed.

## Application consumption

For a new application, start with ProjectCloner. This packaging work does not
change that starting point. The template's automatic vcpkg integration is a
separate follow-up; existing applications may adopt a source port directly.

Once the preview is published, add a private registry entry pinned to the
registry commit containing this version. List `dsqt` in the application's
manifest, alongside its own dependencies:

```json
{
  "dependencies": [
    { "name": "dsqt", "version>=": "0.3.0-waffles.1" }
  ]
}
```

The registry baseline also controls which version resolves; a minimum version
is not an exact pin. Do not point to the older baseline and expect the preview
to be present. The preview uses `version-semver`, while the older package used
numeric `version`; vcpkg does not compare those schemes. Update the application's
minimum version and registry baseline together when opting into this preview.
For development before publication, use the staged overlay
reported by `Test-DsqtPort.ps1 -StageOnly`.

Select the same Qt prefix and triplet in the application's CMake preset, then:

```cmake
find_package(Dsqt CONFIG REQUIRED COMPONENTS Core Bridge Waffles Qsys)
target_link_libraries(myapp PRIVATE Dsqt::Dsqt)
dsqt_deploy_runtime_dlls(myapp)
```

The existing public component names remain unchanged. The actual library
filenames now have a `Dsqt` prefix to avoid collisions in a shared package
prefix. Rebuild applications after updating the library and generated QML
metadata. Use the package's deployment helpers instead of assembling runtime
DLL paths manually.

When declaring the application's QML module, pass
`IMPORT_PATH "${DSQT_QML_IMPORT_PATH}"` to `qt_add_qml_module`, as the
ProjectCloner template already does. `QML_IMPORT_PATH` alone is an IDE setting;
the build-time scanner needs the explicit module import path to discover and
link the static DsQt plugins.

Keep ProjectCloner's `Q_IMPORT_QML_PLUGIN` declarations in `main.cpp`; they
register the static plugins at startup. An existing application adding Waffles
must add the declarations for its modules, for example
`Q_IMPORT_QML_PLUGIN(Dsqt_WafflesPlugin)` and
`Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)`. Linking the archive alone does not
guarantee plugin registration.

## Installation layouts

The ordinary build/install workflow remains available:

| Layout | Release libraries | Debug libraries | Package configuration |
| --- | --- | --- | --- |
| Ordinary installation | `lib/Release/` | `lib/Debug/` | `lib/cmake/Dsqt/` |
| vcpkg package | `lib/` | `debug/lib/` | `share/dsqt/` |

`find_package(Dsqt)` resolves the appropriate layout. vcpkg stores QML metadata
under `share/dsqt/qml/`; ordinary installs retain `qml/`. The package disables
user CMake registry registration and avoids copying dependency-owned libraries
into the shared prefix.

Adding a `-vcpkg` mode to `Library/build_and_install.bat`, ProjectCloner registry
configuration, and a preview publishing workflow remain follow-up work. There
is currently no `build_and_install.sh` in this repository.

## Local validation of this preview

Validated October 15, 2026 with Qt 6.12.0, MSVC 14.51 x64:

- The vcpkg port builds and installs both Debug and Release. Its reinstall
  restores the same package ABI from the isolated binary cache.
- A Release consumer compiles public Core and Qsys headers, links all seven
  components, and imports all seven QML modules from the vcpkg installation.
- The ordinary Release build/install and the same consumer pass with the
  existing split-config layout. Both consumer launches pass a matching-kit
  DLL/plugin dependency preflight.
- A mismatched pinned Qt version is rejected; the matching version is accepted.
- Ninja Multi-Config configures successfully with library tests enabled.

Logs and diagnostic consumer sources are under
`Library/build/vcpkg-validation/`. These checks do not establish registry
publication, a new hosted CI result for these uncommitted packaging changes,
or physical-device acceptance.
