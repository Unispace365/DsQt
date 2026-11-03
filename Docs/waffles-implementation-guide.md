# Implementing an application with Waffles

This guide walks a developer who knows Qt/QML through integrating the current
Waffles components into an application. It distinguishes **DsQt library work**
from **application work**, using AssetViewer and ECPresenter as references.
Read the [overview](waffles-overview.md) first for terminology and responsibilities.

This guide follows the `af/waffle-glass` working branch and the locally committed
ECPresenter integration, reviewed October 12, 2026. For application setup, use
a published package through ProjectCloner; the registry instructions below use
`0.3.0-dev.224.af-waffle-glass` as a verified package example. See the
[backlog validation record](waffles-backlog.md#validation-record) for completed
checks and remaining application/device acceptance.

## 1. Start with ProjectCloner or an existing DsQt project

There are two integration paths:

| Your situation | Required starting point |
| --- | --- |
| **New application** | Always create the project with **ProjectCloner**, then add the Waffles features it needs. |
| **Existing DsQt application** | Integrate Waffles into that project, preserving its existing application behavior. |

### New application: use ProjectCloner

1. Complete the [Windows setup and ProjectCloner instructions](../README.md#getting-started).
   A separate DsQt library build/install is unnecessary for a registry consumer.
2. Run ProjectCloner, choose **Published vcpkg package**, and click **Refresh**.
   Select a supported published version; development packages require an explicit
   choice. `0.3.0-dev.224.af-waffle-glass` is a published Waffles example.
3. Select **Waffles** alongside required **Core**. Add **Bridge** for CMS content
   and any other modules the application needs. Review the Qt requirements and
   select a compatible installed kit; this package requires Qt 6.12.0 or newer
   compatible SDK, including Canvas Painter.
4. Select a destination folder and application name, then click **Clone**.
   ProjectCloner uses [ClonerSource](../Examples/ClonerSource/) to create the
   startup code, settings, assets, exact package pins, and source-specific presets.
5. Work in the generated application. Inspect its configure/build presets and
   follow section 2, then add the product features described in the remaining sections.

### Existing DsQt application: integrate in place

1. Establish the application's current build and runtime behavior. Identify its
   DsQt modules, entry point, main view, settings, content sources, and deployment
   rules before extending them.
2. Check compatibility with the current Waffles SDK requirements. Keep the
   application's existing dependencies and add any missing Core/Waffles module
   links, static QML plugin imports, and runtime resources. Include Bridge when
   the application needs the Bridge content workflow.
3. Add the Waffles stage to the intended application view. Connect content,
   navigation, input, and lifecycle handling to the application's existing
   services, initializing shared services only once.
4. Merge the required Waffles settings and assets into the existing deployment.
   Preserve application-specific values and choose which menu, presentation,
   whiteboard, and idle behaviors the product needs.
5. Verify both the added Waffles features and the existing application workflows
   using section 9.

### Reference implementations for either path

AssetViewer and ECPresenter are references only. Use their individual integration
patterns within the project selected above.

| Reference | What it demonstrates | Files to read first |
| --- | --- | --- |
| [AssetViewer](../Examples/AssetViewer/) | Local/sample media, controls, and styling without a live Bridge workflow. | `main.cpp`, `Main.qml`, `qml/MainView.qml`, `settings/` |
| [ECPresenter](https://github.com/Unispace365/ECPresenter) | Live Bridge content, presentations, custom-layout navigation, quick-menu actions, and idle/resume behavior. | Local `main.cpp` and `qml/MainView.qml` |

Keep product behavior in the application. Add or change a DsQt component when
the capability should be reusable by multiple applications. ECPresenter's action
names, content schema, platform IDs, and asset paths are examples to adapt.

## 2. Prepare the build and runtime

Use Windows x64 with the MSVC 2022 toolchain, CMake 3.29 or newer, Ninja, Git LFS,
and vcpkg as described in the [repository setup](../README.md). Build the library
and application with matching Qt SDKs, architecture, and Debug/Release configuration.

| Dependency | Requirement for the current Waffles implementation |
| --- | --- |
| Qt | **6.12 or newer compatible SDK**; Core and Waffles request Qt 6.12 in CMake. Older presets are not sufficient for this branch. |
| Canvas Painter | Required by the C++ whiteboard and Core's quick-menu dependency. |
| QtCanvas2D | Required at runtime for `DsQuickMenuSegment`. This QML API is a technology preview; verify that the SDK and deployed imports include it. |
| Multimedia | Required for media playback and camera/capture inputs. |
| WebEngineQuick, PDF, Virtual Keyboard | Optional discovery in the Waffles build, but necessary when the application uses those features. Verify the installed SDK modules and deployed runtime. |

The API names are **Canvas2D** (`import QtCanvas2D`) and **Qt Canvas Painter**,
not “QCanvas.” See [Qt's module documentation](https://doc-snapshots.qt.io/qt6-6.12/qtcanvas2d-qmlmodule.html)
and [the API introduction](https://www.qt.io/blog/canvas2d-new-qml-canvas-element-using-qt-canvas-painter).

### Use the published package by default

The generated registry preset lets vcpkg build DsQt against the selected Qt kit
or restore compatible binaries from its cache. It does not use a separately
installed `Documents/DsQt` library. In a matching x64 MSVC developer shell:

```bat
set "VCPKG_ROOT=C:\vcpkg"
cmake --preset ninja-vcpkg-6.12.0
cmake --build --preset ninja-vcpkg-6.12.0-release
cmake --install build/ninja-vcpkg-6.12.0 --config Release
```

These names apply to a generated Ninja/Qt 6.12.0 registry selection. Use the
actual names listed in your app. For another published version, Qt kit, or
source mode, follow [Switching DsQt packages, versions, and Qt kits](articles/switching_dsqt_packages.md).
Existing applications can adopt these helpers and pins without being recreated.

### Local library development alternatives

When changing DsQt itself, use `Library/build_and_install.bat -vcpkg -qt 6.12.0`
and select its completed local package in ProjectCloner. This uses a local source
snapshot and binary cache; it does not publish to the registry. See [local package
setup](vcpkg.md#build-a-local-package-and-use-it-in-a-new-application).

An ordinary installed library remains available through
`Library/build_and_install.bat -qt 6.12.0`. It builds Debug and Release and installs
to `%USERPROFILE%\Documents\DsQt` by default. Select **Installed library** in
ProjectCloner and match its exact Qt kit. Rebuild/reinstall after library changes.
For local paths, see [CMake user presets](articles/cmake_user_presets.md).

### Configure the application

For either integration path, ensure the application requests `Core` and `Waffles`
from the selected package, for example with
`find_package(Dsqt REQUIRED COMPONENTS Core Waffles)`, and links `Dsqt::Core`
and `Dsqt::Waffles`. Preserve other components the project already uses. For a
Bridge application, also request `Bridge` and link `Dsqt::Bridge`.
Registry and local-package helpers set the package paths automatically. Only the
ordinary installed-library mode needs both Qt and DsQt prefixes in
`CMAKE_PREFIX_PATH`. Follow the [library integration guide](articles/migration_subdirectory_to_library.md)
for plugin imports, QML import paths, and deployment scaffolding.

From your generated or existing application directory, use its actual presets
and build directory in these commands:

| Step | Command |
| --- | --- |
| Inspect available configure/build presets | `cmake --list-presets` / `cmake --list-presets=build` |
| Configure with the required Qt SDK and selected DsQt package | `cmake --preset <configure-preset>` |
| Build Release | `cmake --build --preset <release-build-preset>` |
| Install the runnable package | `cmake --install <build-directory> --config Release` |
| Run the deployed app | Run your executable from the installed `bin` directory reported by CMake. |

Replace the angle-bracket placeholders with values from your project. Use
`CMakeUserPresets.json` for local SDK and installation paths. The generated
template or an existing application may still name older Qt SDKs; check that the
selected configuration resolves the required version before building.

To run ECPresenter as a reference, use its
[README](https://github.com/Unispace365/ECPresenter#readme). Its local
`ninja-multi-6.12` presets and `appECPresenter.exe` executable are specific to that
reference application.

### Bootstrap services and graphics

Review the generated or existing entry point for static QML plugin imports,
`DsGuiApplication`, `DsQmlApplicationEngine`, settings initialization, and QML
loading. Add missing initialization while preserving the application's existing
services. Consult the reference entry points for individual integration patterns.
A Bridge app also creates `DsBridgeSqlQuery` with the engine; reuse that service
if the application already initializes it.
Set the virtual-keyboard environment before creating the application;
the references use `QT_IM_MODULE=qtvirtualkeyboard` and
`QT_VIRTUALKEYBOARD_STYLE=waffles`.

The reference graphics configurations differ: AssetViewer initializes WebEngine
and uses D3D11/8-bit color; the current local ECPresenter uses D3D12/10-bit color
and initializes WebEngine when the installed DsQt package includes it. Treat graphics/backend choice
as an application integration decision. Validate web rendering and color on the
chosen combination; do not assume the ECPresenter entry point proves web support.

## 3. Create a stage and connect content

`DsWaffleStage` is the reusable host. The application supplies its background,
launcher, launcher model, and template mapping. It calls stage methods rather
than constructing and destroying internal viewers itself.

### Integration properties

These are the main application-facing properties, not an exhaustive API listing.

| Property | Type | Default | Required | Purpose |
| --- | --- | --- | --- | --- |
| `backgroundContent` | alias to `background.data` | Empty | No | Items shown beneath presentations and included in the glass backdrop. |
| `launcher` | `Component` | `DsTestLauncher` | No | Replace with `DsContentLauncher` for the production browsing panel. Keep a valid component; the current stage assumes one exists. |
| `launcherModel` | `var` | `null` | For content browsing | Adapter supplied to the launcher when it is created. |
| `launcherLayer` | `string` | `"modal1"` | No | Named layer containing the launcher. `"modal3"` places it above fullscreen content. |
| `viewer` | `Component` | `TitledMediaViewer` | No | Replaceable media-window appearance and controls. |
| `playlistTemplateByTypeUid` | `var` | Empty object | For custom slide types | Maps CMS type IDs to QML `Component` objects. Unmapped types use `DsMediaTemplate`. |
| `whiteboard` | `Component` | `Whiteboard` | No | Replaceable board derived from `DsWhiteboard`. |
| `activeWhiteboard` | read-only `DsWhiteboard` | `null` until created | No | Retained drawing instance. |
| `whiteboardShown` | read-only `bool` | `false` | No | Lets the application suppress conflicting menus or input while drawing. |

The stage has six named layers, ordered bottom to top: `background`,
`presentation`, `viewer`, `modal1`, `modal2`, and `modal3`. The fullscreen scrim,
fullscreen viewer, and its controller occupy `modal2`. The floating keyboard
uses `modal3`. The whiteboard is a separate retained overlay above those layers.
Use `getLayer(name)` for supported named-layer access; adding arbitrary layers is
not an implemented public API.

### Usage example: a reusable media surface

Place this component inside the application's initialized DsQt window. Set its
required `imageUrl` to an existing local image or a reachable URL. This small
example opens one viewer; it does not configure Bridge or application lifecycle.

```qml
import QtQuick
import Dsqt.Waffles

Item {
    id: root
    required property url imageUrl

    DsWaffleStage {
        id: stage
        anchors.fill: parent
        launcherButtonVisible: false
        launcher: Component {
            DsContentLauncher { shown: false }
        }
        backgroundContent: [
            Rectangle { anchors.fill: parent; color: DsTheme.surface }
        ]
    }

    Component.onCompleted: stage.openViewer({
        "model": {
            "title": "Example image",
            "media": { "filepath": root.imageUrl.toString(), "type": "image" }
        },
        "matchAspectRatio": true,
        "stage": stage
    })
}
```

`openViewer(properties)` is the application entry point and currently delegates
to `createViewer(properties)`; neither returns a viewer instance. Include `stage`
when using the default Waffles viewer. Preserve the full media descriptor where
available, including dimensions and crop information. Use `closeViewer(viewer)`
for an existing instance so exit animation and stage bookkeeping run.

## 4. Connect Bridge and route selections

Use this section when the application uses Bridge content. Applications with
local or other content sources can keep their existing content services and
supply the stage with the corresponding viewer and playlist data.

### Prepare the application schema

A Bridge **schema** defines the content types, fields, and relationships authors
can use in the CMS. A **trait** is a reusable group of fields attached to a type.
An **appKey** is the field key read by application code; type and frame **UIDs**
are identifiers used to select and interpret content.

Prepare this schema before wiring the runtime adapter. ProjectCloner generates
the application project; schema integration is a separate part of adopting the
Bridge content workflow. For a new application, identify its target schema after
creating the project. For an existing application, extend its current schema
while preserving existing content and relationships.

The reviewed reference is
[schema-downstreamdemo](https://github.com/Unispace365/schema-downstreamdemo).
ECPresenter uses its definitions, and
[`DsContentLauncherModel`](../Library/DsQt/Bridge/qml/DsContentLauncherModel.qml)
contains defaults that match them. The reusable **version 0.1.0 package** is
published as a development candidate in the private
[schema-waffles-dsqt repository](https://github.com/Unispace365/schema-waffles-dsqt),
checked out at `C:\dev\schema-waffles-dsqt` on `main`. Start with its
[README](https://github.com/Unispace365/schema-waffles-dsqt/blob/main/README.md) and
[field contract](https://github.com/Unispace365/schema-waffles-dsqt/blob/main/contract.md).
At the repository root, `schema.json` is canonical,
[waffles.blueprint-export.json](https://github.com/Unispace365/schema-waffles-dsqt/blob/main/waffles.blueprint-export.json)
is the importable type/trait set, and
[manifest.json](https://github.com/Unispace365/schema-waffles-dsqt/blob/main/manifest.json)
records versions, assets, IDs, and adapter/template mappings. Copy its referenced
icons when adopting it. Native Bridge validation and deployment remain pending;
DsQt does not automatically load the manifest. Integration remains
tracked in the [schema backlog task](waffles-backlog.md#bridge-schema-integration).

Use the following inventory when comparing an application's schema. These are
current adapter expectations, not a complete portable schema specification.

| Feature | Schema definitions and application mappings to check |
| --- | --- |
| Library browsing and media | Media records and descriptors, folder/container relationships, titles and thumbnails; map types using `typeKindByUid` / `typeKindByName` and fields using `mediaField`, `titleFields`, and `thumbnailFields`. |
| Platform content defaults | Links under `default_content`, `default_interactive_playlist`, and `default_ambient_playlist` for the features used. Match the adapter's platform field properties. The application's `platform.id` selects an actual content record in the target environment. |
| Scheduled event overrides, when used | Event types and schedules with `content`, `interactive_playlist`, and `ambient_playlist` links. Match `interactiveEventTypes` and the adapter's event field configuration. |
| Playlists and slides | Ambient/interactive playlist types, permitted slide children, their order, and template/media fields. Align actual slide type IDs with `playlistTemplateByTypeUid`. |
| Timing and transitions | `ambient_hold_time` and `disable_transition`, including the appropriate type/trait placement. Package 0.1.0 adds the optional `transition` selector for cut/fade/slide transitions; the reviewed application schema lacks it until integrated. Empty selection uses the application's default transition. |
| Custom layouts, when used | Custom-layout, composite-media, and frame definitions; align `customLayoutTypeUid` and `customLayoutFrames`. The adapter reads frame geometry from `<frame UID>_x/y/w/h` and uses fixed fields such as `background_media`. |
| Hotspots, when used | Hotspot geometry, allowed destination links, and parent/child relationships. The current adapter reads `destination` and `hotspot_x/y/w/h` directly; these names are fixed assumptions to preserve or address in adapter work. |

1. Compare the target schema with the definitions needed for the selected
   features. Record missing types/traits, incompatible field values, link targets,
   and ID differences. Schema types for additional templates do not establish
   that those templates are implemented in DsQt.
2. Use the package's `schema.json` as the initial definitions for a new target
   schema, or merge the required definitions into an existing one. From the
   `schema-waffles-dsqt` checkout, run
   `python -B tools/schema_package.py compare <application-schema.json>` for a
   read-only difference report; exit code 2 means review is needed. Attach shared
   traits to the application's platform/event types where appropriate. Configure
   actual locations, platform instances, and allowed events in the target schema.
3. Use [Bridge Blueprint](https://github.com/Unispace365/bridge-blueprint#export-and-import)
   to export/import type groups and referenced traits where appropriate. Check
   all required references. For an empty target without ID collisions, turn off
   **Generate new IDs**. For overlapping definitions, merge deliberately: the
   reviewed importer can regenerate conflicting IDs and does not remap every
   dependent reference. Preserve existing IDs where content depends on them; if
   IDs change, update all schema references and application mappings together.
4. Run native Bridge/Blueprint schema validation and Bridge's existing-content
   validation, then apply the integrated schema to the intended environment using
   the project's deployment process. Author representative records and links,
   then verify that Bridge Sync exposes them with the expected keys and IDs.
5. Complete the runtime wiring below and validate both the selected Waffles
   features and existing content workflows. Keep the schema revision, mappings,
   and adoption instructions with the application so later updates are repeatable.

The package's `interactiveEventTypes` mapping uses **Scheduled Content**. An
existing application such as ECPresenter can retain its own event types and
configure their names instead. The older root schema in `schema-waffles` also
assigns different aspect ratios to two frame IDs shared with the current DsQt
demo. Follow the package's frame migration notes before merging that schema;
the schema comparison tool reports these conflicts. Package checks do not
replace native schema/content or runtime acceptance checks.

### Wire Bridge content into the application

**DsQt provides the adapter; the application owns the wiring.**

1. Run Bridge Sync and confirm that its database and resources match the
   application's `engine.resource.location` and `engine.resource.resource_db`.
   ECPresenter currently sets `engine.bridgesync.launch_bridgesync=false`, so it
   expects synchronization to run separately.
2. Set `platform.id` in `app_settings.toml` to the intended platform record.
   Apply the field, type, event, and frame mappings in `settings/waffles_content.toml`
   as described below. Without that configuration, adapter defaults continue to
   match `schema-downstreamdemo`.
3. Import `Dsqt.Bridge`, create `DsContentLauncherModel`, and assign that object to
   `stage.launcherModel`. ECPresenter's content settings give folders
   `tapAction = "browse"` and playlists `tapAction = "open"`.
4. Supply a `DsContentLauncher` component and handle `openRequested(item)` in
   application code. The stage supplies its required `stage` property and model.
5. Route launcher requests and the stage's `contentRequested(target)` and
   `playlistRequested(target)` signals through one destination handler.

`DsContentLauncherModel` is an exported QML type in `Dsqt.Bridge`. Link
`Dsqt::Bridge`, declare the `Dsqt.Bridge` QML dependency, and use
`import Dsqt.Bridge`. Remove any application-side copy of the adapter and its
CMake `configure_file` workaround when upgrading; ECPresenter now uses the
module directly. Rebuild/install DsQt and reconfigure/rebuild the application.

| Adapter operation | Result and application action |
| --- | --- |
| `navigationFor(target)` | Returns `null`, `{ playlist, slideUid }`, or `{ item }`. Show an application error for an unresolved target. |
| `playlistFor(target)` | Returns a playlist item or `null`; use it when the requested destination must be a playlist. |
| `slidesFor(playlist.record)` | Produces ordered slide descriptors, including `uid`, `typeUid`, `media`, `holdTime`, transitions, raw record, and custom `layout`. |
| `selectPlaylist(item)` | Tracks the chosen interactive presentation. Call after successful opening; ambient playback should not replace the interactive return destination. |

For an asset, pass its title and media descriptor to `stage.openViewer`. Convert
Bridge's `weblink` kind to the viewer's `web` media type. For a playlist or slide,
call `stage.openPlaylist` with `playlistItem`, `slides`, `ambient`, and
`initialIndex`. Resolve a requested slide UID to its current index first.

Search is over loaded menu content and its descendants, not an unrestricted
search of every CMS record. The [content-launcher tour](waffles-tour/content-launcher.html)
details the model contract and schema overrides.

ECPresenter and ProjectCloner both supply `settings/bridgesync.toml`, loaded by
`engine.extra.engine`. The template keeps Bridge connection settings in that
file and launch policy in `engine.toml`. Configure the server, sync directory,
resource database, and platform ID together. If the expected platform is absent,
correct the configuration or connect to the correct Bridge; do not modify the
schema or author duplicate platform content to accommodate the mismatch.

## 5. Register templates and handle hotspots

Register the application's QML Components by renderer name, then let the adapter
map configured Bridge type IDs to those Components:

```qml
Component { id: mediaTemplate; DsMediaTemplate {} }
Component { id: customLayoutTemplate; DsCustomLayoutTemplate {} }

DsWaffleStage {
    playlistTemplateByTypeUid: contentModel.templateComponents({
        media: mediaTemplate,
        customLayout: customLayoutTemplate
    })
}
```

The `[waffles.content.templates."TYPE_UID"]` tables select `renderer = "media"`
or `"customLayout"`. Additional renderer names need a corresponding application
Component. The settings do not load arbitrary QML filenames. Existing applications
can still assign `playlistTemplateByTypeUid` directly.

The Bridge adapter builds layout frames and normalized media rectangles.
`DsCustomLayoutTemplate` uses `[waffles.content.layout].frameUid`, carried in
the adapter's layout descriptor. A specified ID must have authored content on
the slide: a missing frame logs a warning and displays “Layout unavailable,” without
silently substituting a different frame. An empty ID selects the authored frame
closest to the stage aspect ratio. An explicit QML `frameUid` assignment can
override the descriptor. The application still resolves hotspot destinations.

A custom template receives `slide` and may expose separate `active` and `paused`
booleans. Play time-based content only while `active && !paused`, honoring its
autoplay setting. Suspend playback with pause/resume, preserving the current
position; do not use `active = false` to implement pause, because active also
gates input. Templates can emit
`contentRequested(target)` or `playlistRequested(target)`; the playlist viewer
forwards requests only from the active slot to the stage.

The current Waffles path uses the QML component registry. ECPresenter's older
`[playlist.templateMap]` settings are not automatically consumed by that registry.
Do not assume adding a URL to that table registers a Waffles template.

Ambient playlists advance automatically; interactive playlists wait for user
navigation. The controller's `paused` flag stops ambient advancement and is
forwarded to both loaded slide templates. The built-in media and custom-layout
templates pause video and animated images, including layout backgrounds.
Manual navigation, drag/pinch, and hotspots remain available. A slide entered
while paused stays paused. Resume honors autoplay and starts a fresh ambient
hold interval. Web-page scripts and external capture feeds are not controlled
by this media transport contract. Custom templates must implement their own
`paused` behavior. See [playlists and transitions](waffles-tour/playlists.html).

When `stage.showAmbient()` succeeds, the stage suspends foreground video and
animated images until ambient has finished closing, including its wake-gesture
shield. Media that was playing resumes from its paused position or frame; media
already paused or stopped stays that way. Replacing the ambient playlist does
not briefly resume foreground playback. Foreground media that loads while
ambient is open is held until ambient closes. Ambient media itself remains free
to play. A failed ambient creation does not suspend foreground playback.

This uses a separate `playbackSuspended` property, preserving the playlist's
user-controlled `paused` setting. `DsWaffleStage.foregroundPlaybackSuspended`
exposes the current stage state. Stage-created viewers that expose
`playbackSuspended` are bound automatically, including fullscreen viewers.
`DsPlaylistViewer` forwards the property to loaded templates; built-in media
and custom-layout templates forward it to every nested `DsMediaViewer`.
Application-provided templates should expose a `playbackSuspended` boolean
(default `false`) and forward it to their `DsMediaViewer` instances, or implement equivalent
pause-and-restore behavior for their own backend. Do not overwrite the user's
pause setting to implement this temporary suspension. Web-page scripts and
external capture feeds still require an application-specific transport policy.

## 6. Add quick-menu actions and whiteboarding

### Add the standard Waffles menu

Import `Dsqt.Waffles` and place the host above the stage in the visual stack.
The application supplies its stage and ambient-content callback:

```qml
DsWaffleQuickMenu {
    id: quickMenu
    anchors.fill: parent
    stage: waffleStage
    ambientAvailable: contentModel.ready && !!contentModel.currentAmbientPlaylist
    onAmbientRequested: mainView.playAmbient(true)
}
```

The host handles right-click and five-finger activation (including Shift+mouse
simulation), placement, selection forwarding, and closing. It disables activation
while the stage's whiteboard or ambient view is shown. Hidden or disabled hosts
close their menus. `closeMenus()` explicitly closes all menus; `openAt(point)`
opens a menu at a point **local to the host**.

The nine defaults, in order, are `content`, `whiteboard`, `forward`, `arrange`,
`close_assets`, `gather`, `back`, `asset_browsing`, and `ambient`. Availability
follows stage state. Content opens the launcher's content view; Asset Browsing
opens its search view. Ambient emits `ambientRequested()` for the app to handle.

### Change entries, order, and actions

Set `actionIds` to add, remove, or reorder wedges. Set `actionOverrides` by ID
to replace `iconText`, icons (`iconSource`, or `icon` and `iconPath`),
`available`, or `trigger(position)`. A custom ID needs a callback; an explicit
`available: false` prevents dispatch. Use bindings or replace override objects
when changing them at runtime.

```qml
DsWaffleQuickMenu {
    anchors.fill: parent
    stage: waffleStage
    actionIds: ["content", "whiteboard", "help", "ambient"]
    actionOverrides: ({
        content: { iconText: qsTr("Browse") },
        help: {
            iconText: qsTr("Help"),
            iconSource: "qrc:/images/help.svg",
            available: true,
            trigger: position => helpPanel.openAt(waffleStage.mapFromGlobal(position))
        }
    })
    ambientAvailable: true
    onAmbientRequested: mainView.playAmbient(true)
}
```

Here `helpPanel` is an application-defined panel. Callbacks and
`itemSelected(index, position)` receive **global** positions; convert with
`stage.mapFromGlobal(position)` before calling stage methods.
A `trigger` override replaces the standard action's behavior.

### Appearance and activation

Waffles defaults to bundled Roboto at 16 px and `DsTheme.accent`.
Set `config: ({fontFamily: "Roboto", fontSize: 20})` for larger labels.
The circular visual wraps to two lines and fits text inside padded wedge bounds;
long labels can shrink or elide. Standard action icons are bundled in Waffles,
and close/selection icons in Core. Applications need no `quick_menu` directory
for the defaults. `config.closeIcon`, `config.closePressedIcon`, and
`config.selectionArrow` override the control icons.

Use `rightClickEnabled` and `touchEnabled` to choose activation methods.
`minimumTouchCount` defaults to 5, `minimumClusterSeparation` to 300,
`clusterSize` to 300, and `triggerTime` to 0.1 seconds; configure these at creation.
`edgeMargin` defaults to 8. `[quickMenu] closeDelay` is in milliseconds:
`0` disables automatic closing, and pointer interaction pauses the countdown.

Set `menuComponent` to change the visual for both mouse and touch. This example
also needs `import Dsqt.Core`:

```qml
menuComponent: Component {
    DsQuickMenu {
        innerRadius: 140
        outerRadius: 260
        startAngle: 20
        closeTimer: 10000
    }
}
```

A completely different visual must be an `Item` centered on its local origin
and expose:

- Properties `model`, `config`, `highlightColor`, and `outerRadius` (placement radius).
- Methods `openMenu(touch)`, `closeMenu()`, `updatePoint(localPoint)`, and `releaseGesture()`.
- Signals `itemSelected(int index, point globalPosition)` and `closed()`.

The host binds model/config/color, forwards input, and unloads the visual after
`closed()`. Emit `closed()` after dismissal completes. The visual owns its hit
areas, underlying-input blocking, and availability checks. `DsQuickMenu`
implements this contract, including label fitting.

### Reuse the pieces independently

**Core** exports `DsQuickMenuHost` for activation/lifetime without Waffles.
Supply `model` entries with `iconText`, optional icons and `available`, plus
`config` and an `onItemSelected` handler.

**Waffles** exports `DsQuickMenuActions` for the standard entry model and
`trigger(index, globalPosition)`. `DsWaffleQuickMenu` combines it with the host;
`actionController` exposes the controller.

Public stage properties are `activeLauncher`, `activePlaylist`, `canNavigate`,
`mediaViewers`, `hasMedia`, and `mediaLayoutPending`. Operations are
`showLauncher(mode, stagePosition)`, `nextSlide()`, `previousSlide()`,
`arrangeMedia()`, `gatherMedia(stagePosition)`, and `closeMedia()`.
Arrange/gather wait for leaving fullscreen before positioning windows.
Closing media preserves the presentation; whiteboard lifecycle is separate.

ECPresenter demonstrates the integrated menu in `qml/MainView.qml`.
Validate animation and interaction against the
[Figma quick-menu motion](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=454-1302&m=dev).

### Whiteboard lifecycle

Call `stage.openWhiteboard()` to create or reopen the board. It returns the
retained instance when available. `closeWhiteboard()` hides and preserves the
drawing; `closeWhiteboard(true)` clears it. The default toolbar saves a PNG
through a file dialog and reports save errors. A transparent export contains the
drawing, not a screenshot of application content behind it.

The Waffles menu closes before dispatch and disables activation while
`whiteboardShown` is true. ECPresenter retains the drawing and foreground session
under the ambient overlay. Choose reset versus retention deliberately.
For custom toolbars, drawing properties, and save signals, use the
[whiteboard reference](articles/whiteboard.md) and
[whiteboard Figma design](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=715-1584).

## 7. Implement startup, resume, and idle policy

These are **application responsibilities**. Creating a stage does not configure
an attract loop or remember a user's chosen presentation automatically.
The following workflow describes the ambient/interactive behavior demonstrated
by ECPresenter. Apply the steps your product needs, integrating them with any
existing startup and idle handlers.

1. Wait until the content model is ready and an ambient playlist is available
   before auto-starting it. Honor the application's `autoStartAmbient` setting.
2. When interaction ends idle mode, open the selected interactive playlist,
   falling back to the configured default when necessary.
3. Remember the current slide UID per presentation. Resolve it against refreshed
   slides when reopening; retain an index fallback for removed/reordered content.
4. When returning to idle, close menus and show ambient content over the retained
   foreground session. Reset the foreground only when that is the desired policy.
5. Update current-presentation tracking from `playlistOpened(item, ambient)` so
   launcher selections and hotspot navigation follow the same lifecycle.

`stage.clearForeground()` helps clear stage-owned viewers, drawing, keyboard,
and launcher; it leaves the presentation layer alone. It does not close
separate menu hosts or choose an ambient playlist. Use `quickMenu.closeMenus()`
for explicit dismissal. ECPresenter retains its foreground session under ambient.

Use public stage properties such as `activePlaylist` for integration.
Underscore-prefixed members remain implementation details; the shared menu does
not require application code to inspect them.

## 8. Configure appearance and behavior

### Configure Bridge content types and the application frame

New projects start with **ProjectCloner**, which includes
[settings/waffles_content.toml](../Examples/ClonerSource/settings/waffles_content.toml)
and lists it in `engine.toml`. Existing DsQt projects can copy that file into
their settings directory and append its name to the existing
`[engine.extra].app_settings` array. Preserve other extra settings entries.
This configures the adapter; it does not add Bridge schema definitions, create
content, or automatically instantiate a Waffles stage in a cloned application.

```toml
[engine.extra]
app_settings = ["waffles_settings.toml", "waffles_content.toml"]
```

All values in `waffles_content.toml` live under `waffles.content` in the merged
`app_settings` collection. The starter matches `schema-waffles-dsqt` and uses
the event type `Scheduled Content`. ECPresenter supplies its own copy using
`Connect Wall Event` and `Explore Room Event` for its existing schema.

| Table / key under `waffles.content` | Meaning |
| --- | --- |
| `types."TYPE_UID"` | Registers a Bridge **type ID**, with `kind` (`media`, `folder`, `playlist`, or an app-handled kind), `tapAction` (`open` or `browse`), and optional descriptive `label`. Multiple IDs can share one kind. Labels document the type; filter labels are configured separately. |
| Per-type `mediaField`, `titleFields`, `thumbnailFields` | Override global field defaults for that type. Media descriptors determine image/video/PDF/web behavior. |
| Per-type `playlistMode` | `interactive` or `ambient`; the app passes the resolved item's mode to playlist playback. |
| `unknownTypePolicy` | `hide` limits launcher rows/search matches to entries in `types`; `show` retains legacy behavior. Per-type `launcherVisible = false` hides an explicitly registered type too. |
| `platform` / `events` | Content and ambient/interactive playlist link fields; `events.typeNames` identifies the scheduled event types. Optional `fieldOverrides` retains the adapter's per-type-name content/playlist field overrides. |
| `launcher.filters` | Ordered search buttons with `label`, optional `kinds`, and optional `typeUids`. Both selectors must match when both are supplied; a label-only button clears them. |
| `playback` | Hold-time/transition field keys and `defaultHoldTimeSeconds`. |
| `templates."TYPE_UID"` | Renderer name; custom layouts also declare a `frames` array containing each frame UID and aspect geometry. |
| `layout.frameUid` | The frame this application uses. `t1bv0BSfh7g6` is 16:9 and `nDpUGKOMJHUr` is 80:27 in the current package. Empty enables automatic aspect selection. Verify IDs against the target schema before using them. |

For example, an additional document type can use a different media field:

```toml
[waffles.content.types."YOUR_DOCUMENT_TYPE_UID"]
kind = "media"
mediaField = "document"
titleFields = ["document_title", "record_name"]
thumbnailFields = ["cover_image"]
tapAction = "open"

[waffles.content.layout]
frameUid = "t1bv0BSfh7g6"
```

Replace the placeholder with the application's actual type ID. Registering a
type does not add its records to the menu: platform/event content links still
select the loaded library. Hiding a type only filters launcher rows and search
matches. Playlist slides, composite media, and hotspots remain available to
renderers; the starter need not expose those internal records as launcher rows.

Public adapter properties remain writable from QML. Direct assignments override
their settings-backed defaults; per-type field and tap-action rules are more
specific than the global fallbacks. With no content settings, previous defaults
remain available. The adapter observes settings rebuilds; an already-open playlist
retains its slide descriptors, so reopen it after editing layout configuration.
Native schema and content validation remain separate steps.

### Configure appearance

Load `waffles_settings.toml` through `[engine.extra].app_settings`. Waffles reads
the resulting `app_settings` collection; a separate collection called `waffles`
does not replace this connection.

| Setting or property | Consumer | Purpose / status |
| --- | --- | --- |
| `[waffles.theme]` | DsQt | Colors, glass, and `uiScale` for interface controls. |
| `[waffles.viewer]` | DsQt | Viewer size limits, initial dimensions, and open/close animation. |
| `[waffles.fullscreen]` | DsQt | Fullscreen margins and scrim appearance. |
| `[waffles.keyboard]` | DsQt | Keyboard style and locales. |
| `[waffles] controlsIdleSeconds` | DsQt | Delay before eligible viewer controls hide; `0` disables. |
| `[waffles.playlist] defaultTransition`, `transitionDuration`, `crossfadeMs` | DsQt | Per-slide transitions and whole-playlist crossfade. |
| `[waffles.playlist] autoStartAmbient` | ECPresenter | Whether the application auto-starts/returns automatically to ambient playback. |
| `[engine] idle_timeout` | Engine, handled by app | Inactivity threshold in seconds. |
| `[quickMenu] closeDelay` | DsQt Core menu | Menu close delay in milliseconds, outside the `waffles` table. |
| Launcher tap policy | Bridge adapter / app | Per-type `tapAction` in `waffles_content.toml`; `drillableKinds` remains the fallback for types without a rule. |
| Fallback slide duration | Bridge adapter | `[waffles.content.playback].defaultHoldTimeSeconds`; positive slide timing takes priority, followed by platform timing. |

Use [theming and settings](waffles-tour/glass-and-theming.html) for detailed
values. Deploy the application's required icons and fonts as well as the QML
modules. Standard media/window controls, quick-menu icons, and Roboto are bundled with
DsQt; application launcher and legacy assets still resolve under `%APP%`.

## 9. Verify the integrated application

### Content state and recovery

Core's `DsMediaViewer` exposes `loadState` (`empty`, `loading`, `ready`, or
`error`), `loading`, `errorString`, `mediaLoaded()`, `mediaFailed(detail)`, and
`retry()`. Retry recreates the backend at the same URL, allowing a missing file
to load after synchronization completes. It does not start Bridge Sync or
change the source record. Diagnostic details go to the log.

Waffles' `DsContentState` supplies the themed loading/empty/error treatment used
by media windows and built-in slide templates. It follows the
[missing-content design](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=47-1217)
using the bundled document icon, Waffles typography, and a retry action. A custom
renderer can place `DsContentState { anchors.fill: parent; viewer: mediaViewer }`
over its Core media viewer. Leave navigation and relevant hotspots available.

Application resolvers must handle missing destinations. Call
`stage.showContentError(qsTr("Content unavailable"), qsTr("Please choose another item."))`
when a link cannot be resolved. ECPresenter also handles empty presentations and
removed linked slides; a failed request leaves the current presentation running.
The launcher distinguishes loading content, an empty library, and an empty
folder; the folder state offers Go back. A missing explicit layout frame never
silently selects another frame.

### Installed file check

After building and installing the app, run the read-only
[deployment validator](../Tools/ValidateWafflesDeployment.ps1):

```powershell
& C:\dev\DsQt\Tools\ValidateWafflesDeployment.ps1 `
  -BuildDirectory C:\dev\ECPresenter\build\ninja-multi-6.12 `
  -DeployBin C:\dev\ECPresenter\build\ninja-multi-6.12\DEPLOY\Release\bin `
  -Configuration Release
```

Use your own build/deployment paths. `-Features Web,Pdf,Keyboard,Touch` is the
default; list only features required by that application. `-AssetGroups` names
the app's icon directories (defaults: `quick_menu,content_launcher,window`). Legacy
`qml/ux` window controls still use app-provided icons; other window controls bundle
their icons in Waffles. The script
checks deployed Qt versions and x64 binaries against the CMake cache, Canvas2D,
media backends, optional modules, MSVC runtime availability, source/deployed
icons, and standard settings filenames. It warns about missing extra settings
hooks and developer environment overrides. It does not parse every TOML override,
verify static plugin registration, or contact Bridge.

The template and ECPresenter link/import `Dsqt.Touch` when available so the
Core shell's optional touch-debug controls resolve. This is separate from
TouchEngine and does not activate touch filtering. The installed DsQt package
exports its WebEngine/PDF/Virtual Keyboard feature dependencies; consumer entry
points initialize WebEngine conditionally before constructing the application.

Prepare a matching runtime before launching: use the build's Qt version,
compiler, x64 architecture and Debug/Release configuration; verify DLLs and QML
imports, and set any child-process PATH in the same invocation as the launch.
For installed acceptance, remove developer QML/plugin overrides and use the
deployed files. Debug additionally requires the matching non-redistributable
compiler runtime. Do not change the machine-wide PATH.

### Application and device acceptance

| Check | Expected evidence |
| --- | --- |
| Build and deployment | Library and app use the same SDK/configuration; run from the installed package, with Canvas2D and optional feature imports present. |
| Existing application workflows | Startup, content access, navigation, settings, input, idle behavior, and deployment still work after adding Waffles; shared services initialize once. |
| Bridge schema integration, when used | Required definitions are deployed; authored records synchronize with the expected keys, types, frame IDs, and links; existing content remains valid and application mappings match the integrated schema. |
| Content startup | Correct platform/database; nonempty content where expected; startup with missing or delayed content behaves visibly. |
| Navigation | Folder drill-down, search, asset opening, presentation changes, and hotspot links all resolve correctly. |
| Presentations | Interactive navigation and hotspots while paused, media pause/resume without position reset, ambient timing, template registration, slide resume after switching, and content reorder/removal behavior. |
| Touch and menus | Five-touch/right-click activation, disabled actions, coordinate mapping, close timing, and physical target sizes on the actual display. |
| Whiteboard | Concurrent strokes, history, close/reopen, PNG save failure/success, transparent export, and idle reset. |
| Media and focus | Video/audio, PDF page/reset/lock, web input, keyboard focus, fullscreen, and capture-device disconnect/retry where used. |
| Lifecycle | Repeated interactive/ambient transitions leave no stale windows, drawing, menus, or keyboards. |
| Graphics and design | Working GPU rendering, stable memory/frame timing, correct colors, and agreement with the Figma design. |

Use QML tooling and behavior tests for the paths you change. A passing test run
with “No QRhi found” does not validate Canvas2D/Canvas Painter appearance or GPU
performance. Record software checks and physical-device acceptance separately.

## Troubleshooting and remaining limitations

| Symptom | First checks |
| --- | --- |
| `QtCanvas2D` or CanvasPainter missing | SDK is 6.12+ with the module installed; rebuild/reinstall DsQt and check deployed QML imports. |
| DsQt types unavailable or stale | Installed library matches the app build; static plugins and import paths are present; new QML files are listed in `QML_FILES`. |
| Empty launcher | Bridge Sync is running; configured settings filename exists; database path, platform ID, schema fields, and adapter `ready` state are correct. |
| Custom layout uses the basic media template | Actual slide `typeUid` matches `playlistTemplateByTypeUid`; adapter frame/layout configuration matches the schema. |
| Hotspot does nothing | Destination exists and both navigation signals are connected to the application resolver. |
| Menu has missing icons | Rebuild/install Core and Waffles for bundled defaults; verify any custom icon URLs in overrides/config. |
| Settings seem ignored | File is merged into `app_settings`; property reads use the right prefix; the setting is actually wired in code. |
| Web viewer is unavailable or blank | WebEngine module, initialization, deployed resources, and chosen graphics backend have been validated together. |

Additional template families and the proposed new schema demonstration app are
on hold. Application-schema adoption and device acceptance remain delivery work.
Consult the reconciled [backlog](waffles-backlog.md) for ownership and status. Use the
[Source Tour](waffles-tour/index.html) for deeper component details and the
[installer guide](articles/installer_guide.md) for packaging.
