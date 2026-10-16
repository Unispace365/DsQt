# DsQt source package for Waffles

The package name is `dsqt`. Both preview and main releases use the manifest's
`version` field, which accepts prerelease suffixes and four numeric components.
`version-semver` accepts the preview but requires exactly three numeric
components, so it cannot represent the main release format used here.

| Source | Published `version` | ZIP and Git tag |
| --- | --- | --- |
| Waffles branch | `0.3.0-waffles.1` | `dsqt-0.3.0-waffles.1.zip`, `library/v0.3.0-waffles.1` |
| Main, Build Library run 241 | `0.3.0.241` | `dsqt-0.3.0.241.zip`, `library/v0.3.0.241` |

`Library/VERSION.txt` keeps the numeric CMake project version `0.3.0`. The source
port manifest keeps `"version": "0.3.0-waffles.1"` as its preview template,
including after merge. **Build Library** selects the preview on the branch and
`0.3.0.<Build Library run number>` on main. The publisher stamps only the
generated registry port; no merge-time manifest edit is needed. The publisher's
own workflow run number does not affect the version.

Branch GitHub releases are marked as prereleases. Once a preview is published,
increment its suffix to `.2`, `.3`, and so on before releasing changed source.
Release tags cannot be moved to different commits; rerunning the same release
for the same commit is allowed. Publishing always pins the exact source SHA.
The source port's zero `REF` is a template replaced during packaging.

See the [vcpkg manifest version reference](https://learn.microsoft.com/en-us/vcpkg/reference/vcpkg-json#version-version-semver-version-date-version-string).

This integration adapts the source-port work from `ph/develop`. The registry's
older `0.2.0.4` package does not contain the current Waffles implementation.
This preview is not published merely by building it locally.

## Select packages and modules with ProjectCloner

ProjectCloner separates the package source, module selection, and Qt kit.
Published versions are read from the private Git registry, including each
version's immutable `dsqt-modules.json`. A generated project pins both the
selected version/revision and the registry commit. Older packages without
module metadata remain visible with an explanation; they cannot be used with
this template until their requirements and template compatibility are verified.

`Library/dsqt-modules.json` declares minimum Qt versions, Qt components, and
module dependencies. The library build, installed package, publisher, and
cloner use this metadata. Installed metadata also records the built modules and
exact Qt version. Local package metadata points to the staged source snapshot,
so a consumer can select additional modules and let vcpkg build them.

Core is required. Bridge, Waffles, Qsys, Spout, Touch, and TouchEngine are optional.
Waffles itself is Bridge-independent; applications add Bridge for CMS content.
Core in this preview uses Canvas Painter, so all current selections still need
Qt 6.12. The UI does not claim that disabling Waffles permits an older Qt kit.

The vcpkg port preserves its full default feature set for existing consumers.
For an explicit subset, disable defaults and list only the desired features:

```json
{ "name": "dsqt", "default-features": false, "features": ["qsys"] }
```

This builds Core and Qsys without Waffles, TouchEngine, or Spout. The features
are `bridge`, `waffles`, `qsys`, `spout`, `touch`, and `touchengine`. Ordinary
library builds expose corresponding `DSQT_BUILD_<Module>` CMake options and
retain the full build by default. Required module dependencies are included
automatically. Exported targets, Qt dependencies, QML metadata, tests, and
runtime DLL deployment follow the enabled/requested modules.

To prepare a local subset through the batch wrapper:

```bat
Library\build_and_install.bat -vcpkg -qt 6.12.0 -modules Core,Qsys
```

Or use `Build-DsqtPackage.ps1 -Qt 6.12.0 -Modules 'Core,Qsys'` from the matching
MSVC developer shell. vcpkg includes the feature set in its binary cache key;
a consumer selecting more features may require another build.

The generated `dsqt-project.json`, vcpkg feature list, and CMake presets agree on
the selection. The template initializes and registers only selected modules.
Waffles files are omitted when disabled; Bridge settings/initialization and the
TouchDesigner example are omitted or disabled independently.

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

## Build a local package and use it in a new application

Start new applications with **ProjectCloner**. Build/install the updated tool
and template with the ordinary `-tools` workflow if your installed copy predates
the local-package preset. Other examples are references, not project templates.

Set `VCPKG_ROOT` to your bootstrapped vcpkg checkout, then run:

```bat
C:\dev\DsQt\Library\build_and_install.bat -vcpkg -qt 6.12.0
```

`-qt` is required; a full kit path such as `D:\Qt\6.12.0\msvc2022_64`
also works. The batch file prepares MSVC x64 and builds both Debug and Release.
Package mode supports `-qt` and optional `-modules`; run ordinary test, tool, and install options
separately. It does not pull/update vcpkg or install into `Documents/DsQt`.

The output is `Library/build/vcpkg-package/qt6.12.0/`:

| Entry | Purpose |
| --- | --- |
| `source/` | Snapshot of tracked and non-ignored Library files, including local edits |
| `overlay/dsqt/` | Local vcpkg recipe pointing to that snapshot |
| `triplets/` | Compiler/architecture/Qt selection used by vcpkg |
| `installed/` | Package builder's Debug and Release installation |
| `dsqt-local-package.json` | Completed-build metadata read by applications |

Generate your application using ProjectCloner. In an x64 MSVC developer prompt
matching the package build, configure the **generated application**:

```bat
set "VCPKG_ROOT=C:\vcpkg"
set "DSQT_LOCAL_PACKAGE_ROOT=C:\dev\DsQt\Library\build\vcpkg-package\qt6.12.0"
cd C:\dev\MyWafflesApp
cmake --preset ninja-vcpkg-local
cmake --build --preset ninja-vcpkg-local-release
cmake --install build/ninja-vcpkg-local --config Release
```

Use `ninja-vcpkg-local-debug` and `--config Debug` for Debug. Deployment is under
`build/ninja-vcpkg-local/DEPLOY/Release/bin/` (or `Debug/bin/`). Application
settings, Bridge content, and application schema setup still follow the
[Waffles implementation guide](waffles-implementation-guide.md).

Before CMake's `project()` runs vcpkg, `cmake/DsqtLocalPackage.cmake` reads the
metadata, selects the exact Qt kit and triplet, and enables the manifest's
`dsqt-local` feature. The generated app installs into its own
`build/ninja-vcpkg-local/vcpkg_installed/` and uses its own `share/dsqt` package.
It does not link to the builder's installed directory or silently choose a
`Documents/DsQt` installation. The application and Library builtin baselines
must match; the helper reports a mismatch before dependency installation.

### Update an application after editing DsQt

Run the same package command again, then configure and build the application
again. Each snapshot gets a local version such as
`0.3.0-waffles.1.local.h0123456789abcdef`; the suffix identifies the source,
recipe, triplet, and baseline. This prevents an already-installed package from
concealing local source changes. It does not change the committed preview
version or create a registry release. The local builder uses the `version`
field too: it appends `.local.h<hash>` to an existing prerelease, or
`-local.h<hash>` to a numeric version. It uses the source manifest as its base;
it does not assign a main CI build number. Unchanged inputs reuse the same
version and cached binaries when the build environment also matches.

Keep the output folder in place: the overlay references its source snapshot
by absolute path. Do not rebuild that folder while an application is configuring
or building. A failed package build removes the ready metadata, so consumers
cannot select an incomplete snapshot. Changing the package root or Qt kit
requires `cmake --fresh --preset ninja-vcpkg-local` in the application.

### Is the package local to this machine?

Yes, by default. The output folder is local, and vcpkg's default Windows binary
cache is `%LOCALAPPDATA%\vcpkg\archives`. `VCPKG_DEFAULT_BINARY_CACHE` or
`VCPKG_BINARY_SOURCES` can select another cache; both builder and consumer honor
the caller's settings. Configure the same cache and compatible compiler,
triplet, and dependencies to reuse the compiled DsQt package across apps.
Different build inputs may correctly cause another compilation.

A team can share a binary cache, but that alone does not distribute this local
source overlay or its absolute paths. A published, commit-pinned source port in
the private registry is the separate team-distribution workflow. See Microsoft's
[binary caching guide](https://learn.microsoft.com/en-us/vcpkg/users/binarycaching).

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

For a new application, use ProjectCloner. Choose a source in its UI; the
generated project contains presets for that source:

| Preset | DsQt source |
| --- | --- |
| `ninja-vcpkg-6.12.0` | Published source package from the private registry |
| `ninja-vcpkg-local` | Working-tree snapshot from `build_and_install.bat -vcpkg` |
| Installation-based presets | Ordinary installation in `Documents/DsQt` |

The registry path is prepared but awaits publication of the Waffles preview and
an updated consumer baseline. `Examples/ClonerSource/vcpkg-configuration.json`
currently pins the verified registry commit
`1e1458981aec5e3a653721c1f706949a46a20963`, which contains `0.2.0.4`.
The registry feature requires `0.3.0-waffles.1` or newer, so it fails instead of
silently consuming that older package. After publication, replace the `dsqt`
registry's baseline with the commit reported by the publishing workflow.
The Microsoft `builtin-baseline` in `vcpkg.json` is a separate dependency pin.

With that published baseline installed in a generated application:

```bat
set "VCPKG_ROOT=C:\vcpkg"
cmake --preset ninja-vcpkg-6.12.0
cmake --build --preset ninja-vcpkg-6.12.0-release
cmake --install build/ninja-vcpkg-6.12.0 --config Release
```

This requires Git access to both private repositories and Git LFS, but no local
DsQt checkout. vcpkg fetches the source commit pinned by the published recipe;
it compiles locally unless a compatible binary cache entry already exists.
The registry distributes recipes and version history. A shared binary cache
is a separately configured service; publishing a port does not upload compiled
DsQt binaries.

`cmake/DsqtRegistryPackage.cmake` selects Qt before `project()`, enables the
`dsqt-registry` manifest feature, and generates the matching MSVC x64 triplet
under the build directory. Its triplet bytes match `New-DsqtTriplet.ps1` for
the same kit. ProjectCloner emits registry presets for each selected Qt version
>= 6.12. For a custom installation root, pass
`-DDSQT_REGISTRY_QT_ROOT=D:/Qt`; changing a configured kit requires `--fresh`.
Registry mode rejects source overlays so the chosen recipe comes from the
registry. Select local package mode when generating a working-tree development
project, or copy the local preset/helper into an existing project.

For an existing DsQt application, add the template's `dsqt-local` manifest
feature and local configure/build presets, copy `cmake/DsqtLocalPackage.cmake`,
and include it before `project()`. Align the builtin baseline and use the
layout-aware runtime helpers below. Existing installation-based presets remain
available without enabling the local feature.

For registry consumption in an existing application, copy the registry
configuration and helper, include the helper before `project()`, and add its
configure/build presets. Either copy the `dsqt-registry` feature or list
`dsqt` directly alongside the application's dependencies:

```json
{
  "dependencies": [
    { "name": "dsqt", "version>=": "0.3.0-waffles.1" }
  ]
}
```

The registry baseline also controls which version resolves; a minimum version
is not an exact pin. Do not point to the older baseline and expect the preview
to be present. Preview and main packages both use `version`; the prerelease
sorts before the corresponding main release. Update the application's minimum
version and registry baseline together when opting into a published package.
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

The local preset uses an overlay and needs no registry publication. There is
currently no `build_and_install.sh` in this repository.

## Publish a preview or main release

`.github/workflows/publish-vcpkg-port.yml` adapts the `ph/develop` publisher.
It is manually dispatched, with preparation as the default, and requires an
exact source commit SHA. For a preview it preserves the source manifest's
`version` (`0.3.0-waffles.1`). For main, supply the four-part version produced
by a successful **Build Library** run, such as `0.3.0.241`. The publisher checks
that the release tag identifies that source SHA, that the commit is on main,
and that the matching main push run completed successfully. It then writes
that version into the generated port. It never rewrites a published
version/revision or uses its own run number.

The workflow requires a valid `REGISTRY_PAT` secret with read access to
`Unispace365/vcpkg-registry`, plus contents write access for publication.
Its automatic GitHub token has `actions: read` permission to verify the main
build. The source commit must include this consumer configuration. GitHub also
requires a manually dispatched workflow to be present on the repository's
default branch before it can be dispatched; the selected execution ref and
source commit can then be the Waffles branch.

1. Commit and push the approved changes. Record the exact source SHA.
2. For a main release, first wait for **Build Library** to succeed and record
   its four-part release version and tagged SHA. Dispatch **Publish vcpkg Port**
   with the source SHA and expected preview or main version. Leave `publish`
   false. Review the `dsqt-registry-preview` artifact, including the pinned
   source REF, version/baseline changes, and `publication/build.json` for main.
   Preparation creates no commits and pushes nothing.
3. Approve the two exact registry commit messages, then dispatch with the same
   source SHA/version, those messages, and `publish` true. The workflow checks
   for duplicate versions, backward baseline changes, and concurrent registry
   updates before pushing normally to `main`.
4. Review the `dsqt-registry-consumer` artifact. Its configuration pins the
   newly published registry commit. Apply that baseline to ClonerSource and
   consuming apps, then validate a clean registry-based application build.

Existing applications pinned to older registry commits retain those baselines.
Publishing changes the registry's default DsQt version at the new commit;
applications opt into it by updating their registry pin. Bump the preview
number (or the port revision for recipe-only fixes) before the next publication.
Main releases use the next successful Build Library run number, so gaps are
normal. Merging selects the main release format; registry publication remains
a separate manual action.

See the [vcpkg registry documentation](https://learn.microsoft.com/en-us/vcpkg/concepts/registries)
for version database rules and
[GitHub workflow dispatch documentation](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax#onworkflow_dispatch)
for manual workflow availability.

## Local validation of this preview

Validated October 15-16, 2026 with Qt 6.12.0, MSVC 14.51 x64:

- The vcpkg port builds and installs both Debug and Release. Its reinstall
  restores the same package ABI from the isolated binary cache.
- A Release consumer compiles public Core and Qsys headers, links all seven
  components, and imports all seven QML modules from the vcpkg installation.
- The ordinary Release build/install and the same consumer pass with the
  existing split-config layout. Both consumer launches pass a matching-kit
  DLL/plugin dependency preflight.
- A mismatched pinned Qt version is rejected; the matching version is accepted.
- Ninja Multi-Config configures successfully with library tests enabled.
- `build_and_install.bat -vcpkg -qt 6.12.0` builds both configurations.
  Repeating it with the full Qt kit path preserves the local package version
  and reuses the installation without compiling DsQt again.
- The updated ProjectCloner generates an application whose local presets match
  the template. It restores the exact DsQt package ABI from the binary cache,
  builds in Debug and Release, and deploys Release with its runtime dependencies.
- That generated application passes a 15-second offscreen startup check after
  a matching-kit preflight of 45 binaries. Bridge Sync is disabled and test logs
  stay inside the generated application directory. This checks startup/imports;
  it does not validate GPU rendering or live Bridge content.
- Missing package metadata and mismatched dependency baselines fail before
  dependency installation. Missing Qt selection and unsupported batch option
  combinations return an error.
- ProjectCloner filters installed Qt kits using the selected package and module
  requirements. Local and installed binaries require their exact Qt version;
  older kits cannot be selected for this Qt 6.12 template. Local package mode
  configures successfully with the Git registry configuration present.
- Against the live pinned Git registry, the registry preset fails explicitly
  because `0.3.0-waffles.1` has not been published. It does not fall back to the
  older DsQt package. A dry run against a filesystem registry fixture resolves
  the prepared `0.3.0-waffles.1` recipe and its dependencies.
- A second filesystem registry fixture uses the already-built local package
  version to exercise registry consumption without rebuilding DsQt. The
  generated application restores the matching cache entry, builds Debug and
  Release, deploys Release, and passes the same 45-binary preflight and
  15-second offscreen startup check. This validates the consumer path; it is
  not a test of a published Git registry preview.
- Registry mode rejects simultaneous local mode, source port overlays, and
  Qt versions below 6.12. Its generated triplet matches the producer triplet
  byte for byte so their binary cache identities agree.
- The publisher's YAML parses, its Python blocks compile, and its Bash blocks
  pass syntax checks. Its preparation steps produce the expected port tree,
  version entry, and baseline in a local registry checkout without creating
  commits. Duplicate versions, mismatched requested versions, backward
  baseline updates, and missing publication commit messages are rejected.
- The branch/main versioning update passes 50 local checks. Actual workflow
  code selects the preview on the branch and `0.3.0.241` for main run 241.
  Git/API fixtures verify the main release evidence and reject wrong tags,
  branches, source commits, run numbers, incomplete/failed builds, and duplicate
  publications. Generated ports retain the approved SHA and module metadata;
  the source manifest remains unchanged. The real vcpkg 2026-03-04 executable
  accepts both formats in `version`, accepts their local-hash variants, and
  resolves preview/main ordering with exact pins and minimum constraints.
  This tests the versioning logic locally, not GitHub-hosted execution.

The module-selection implementation was also checked on October 16:

- A Core/Qsys-only local package builds Debug and Release and records its built
  modules. Requesting Waffles from that installation fails with a missing
  component error.
- ProjectCloner generates Core-only, Core/Qsys, Core/Waffles, and all-module
  applications. Their module lists and vcpkg feature lists agree; omitted
  modules lose their initialization, plugins, settings, and starter QML files.
  Core/Waffles does not select Bridge. A fresh generation with project-level
  files enabled also passes.
- Core-only, Core/Qsys, Core/Waffles, and all-module generated applications
  build and pass 10-second offscreen startup checks. Their link commands match
  the selected modules. These checks exposed and corrected conditional target
  scopes in the package exports; the Waffles and Qsys consumers use regenerated
  exports, and the final Core/Qsys package is rebuilt from the corrected source.
  A generated application then restores that final package from the binary
  cache through its local vcpkg preset, builds Release, and passes startup.
- Live registry discovery reads all four currently published versions. Their
  absent module metadata is displayed explicitly and prevents generation with
  an unverified package. The preview remains unpublished.
- The production ProjectCloner UI builds and launches after a matching-kit
  runtime preflight. A Qt window capture records the actual selector UI.

Logs and diagnostic consumer sources are under
`Library/build/vcpkg-validation/`. These local checks do not establish registry
publication, hosted CI results for the current uncommitted changes, or
physical-device acceptance. A clean build against the live Git registry remains
pending publication and the consumer baseline update.
