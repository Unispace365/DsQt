# Waffles overview

Waffles is the reusable touchscreen interface in DsQt. It supplies a stage for
presentations, draggable media windows, a content launcher, drawing tools, and
shared styling. An application such as ECPresenter connects those components to
its content and decides how the experience behaves.

This overview describes the implementation reviewed on October 12, 2026. For
setup and integration steps, use the [implementation guide](waffles-implementation-guide.md).

## Choose your integration path

| Your situation | Starting point |
| --- | --- |
| **Creating a new application** | Always use **ProjectCloner**, the DsQt tool that generates a named project with build files, application structure, and settings. Then integrate the Waffles features you need. |
| **Adding Waffles to an existing DsQt application** | Work in the existing project. Add the required Waffles dependencies and application connections while preserving its startup, content, settings, and deployment behavior. |

AssetViewer and ECPresenter are **reference implementations** for both paths.
Use them to understand integration patterns. ProjectCloner uses
[ClonerSource](../Examples/ClonerSource/) as its project template.

## The parts and their responsibilities

| Part | What it provides | What the application supplies |
| --- | --- | --- |
| **DsQt Core** | Engine and settings services, media dispatch, touch clustering, quick-menu activation/lifecycle, and a replaceable circular visual. | Application startup, device choices, and custom menu models or visuals. |
| **DsQt Waffles** | Stage, media windows and controls, launcher UI, standard quick-menu actions, presentation playback, custom layouts, whiteboard, camera/capture services, theme, and glass effects. | Content to display, template registration, navigation handling, and session behavior. |
| **DsQt Bridge** | Access to synchronized content and `DsContentLauncherModel`, which converts Bridge records into launcher items, slides, and layout data. | A compatible application schema, field/type mappings, platform selection, and a running Bridge Sync service or process. |
| **ECPresenter** | A reference application combining those DsQt modules with live Bridge content. | Its own startup/idle policy, presentation resume, custom menu actions, ambient selection, content routing, assets, and deployment configuration. |
| **AssetViewer** | A reference application in this repository demonstrating Waffles with local/sample media, without a live Bridge content workflow. | Reference patterns for media, controls, and styling to adapt to the application's needs. |

DsQt contains both QML and C++. The Waffles whiteboard drawing engine is C++;
its host and toolbar are QML. The current Core and Waffles builds require
**Qt 6.12**, including Qt Canvas Painter. The quick menu uses the **Canvas2D**
QML API from `QtCanvas2D`; the whiteboard uses the C++ Canvas Painter API.
[Canvas2D remains a technology preview](https://doc-snapshots.qt.io/qt6-6.12/qtcanvas2d-qmlmodule.html),
so target-device validation is part of delivery.

## Bridge schema ownership

A **schema** defines the content types, fields, and relationships that authors
can create in Bridge. The application must include the definitions required by
its selected Waffles features in its own schema. This applies to both new
applications created with ProjectCloner and existing DsQt applications using
Bridge content. Applications using only local media do not need a Bridge schema.

DsQt supplies the runtime components and adapter. A reusable schema package is
now published as a **version 0.1.0 development candidate** in the private
[schema-waffles-dsqt repository](https://github.com/Unispace365/schema-waffles-dsqt).
It contains 13 content types, four reusable field groups, a Blueprint export,
application mappings, and a
[field contract](https://github.com/Unispace365/schema-waffles-dsqt/blob/main/contract.md).
The checkout is at `C:\dev\schema-waffles-dsqt` on `main`. The schema is at the
repository root; native Bridge validation and deployment remain pending.

The application/schema maintainers own integrating the package, preserving
existing content, and aligning application mappings. ECPresenter currently uses
[schema-downstreamdemo](https://github.com/Unispace365/schema-downstreamdemo),
which also supplies the adapter's default field names and identifiers.

Applications now supply those mappings in **`settings/waffles_content.toml`**,
loaded into `app_settings`. It can describe several content types, their media
fields, browsing/opening behavior, search filters, playlist roles, and template
IDs. A **frame** is an authored custom-layout geometry; `layout.frameUid` selects
the frame this application uses. A specified frame is never silently replaced
when its content is missing. An empty setting enables automatic aspect selection.
ProjectCloner includes the starter file; existing applications add it in place.
See [content configuration](waffles-implementation-guide.md#configure-bridge-content-types-and-the-application-frame).

The [schema integration task](waffles-backlog.md#bridge-schema-integration)
covers the package and its adoption. Static checks and regression tests pass;
individual maintainer assignment, application-schema migration,
native Bridge validation, and device acceptance remain open. The
[implementation guide](waffles-implementation-guide.md#prepare-the-application-schema)
now places schema preparation before Bridge wiring.

## Terms used in the project

| Term | Meaning |
| --- | --- |
| Stage | The application surface managed by `DsWaffleStage`; it places presentations, windows, and overlays. |
| Viewer | A window or component that displays an image, video, PDF, web page, or other supported media. |
| Content launcher | The panel for browsing folders, searching loaded content, and opening an item. |
| Ambient playlist | An automatically advancing presentation shown while the display is unattended. |
| Interactive presentation | A presentation that waits for user navigation. |
| Template | A reusable slide layout. The basic media template and authored custom-layout template are implemented. |
| Hotspot | A touchable area that requests navigation to an asset, slide, or presentation. |
| Glass | A visual treatment that samples and blurs content behind a panel. |

## Implemented capabilities

| Capability | Reusable implementation in DsQt | Integration in ECPresenter |
| --- | --- | --- |
| Content browsing | Launcher UI, folders, breadcrumbs, title search, configurable kind/type filters, and settings-driven Bridge type/field mappings. Search covers the loaded menu tree. | Supplies `waffles_content.toml`, selects the platform, and opens the chosen content. |
| Media windows | Image, video, PDF, and web viewing; movement, sizing, fullscreen, controls, keyboards, and looping. | Converts content descriptors into viewer properties and supplies application assets. |
| Presentations | Playback, transitions, a controller that pauses media and automatic advancement, and custom layouts using an explicit application frame or automatic aspect selection. | Configures playlist roles and template IDs, registers renderer Components, remembers the selected slide, and switches between ambient and interactive modes. |
| Hotspots | Layout hit areas and navigation signals; Bridge destination resolution. | Handles those signals and opens the resolved viewer, playlist, or slide. |
| Quick menu | Core host with five-touch/right-click activation, replaceable visuals, and timed closing; Waffles supplies standard actions and bundled icons. | Creates `DsWaffleQuickMenu` with a stage, chooses optional overrides, and handles ambient content selection. |
| Whiteboard | Pen/highlighter/eraser, multi-touch drawing, colors, sizes, history, clear, backgrounds, and PNG export. | Chooses drawing retention/reset policy. Normal Close and the ambient overlay preserve the drawing. |
| Branding and live inputs | Theme colors, interface scale, glass, and shared camera/capture connections. | Chooses the brand and device configuration and validates the complete experience. Library capture support does not establish a finished application capture workflow. |

## How the application fits together

1. The application initializes DsQt, loads settings, and makes content available.
2. It creates a `DsWaffleStage` and supplies a launcher model and slide templates.
3. A launcher selection or hotspot emits a request. Application code resolves the
   destination and calls the stage's viewer or playlist methods.
4. DsQt manages the resulting components, rendering, controls, and transitions.
5. Application code handles session decisions: returning to ambient content,
   remembering a presentation position, and clearing foreground interaction.

For example, `DsWaffleQuickMenu` combines Core's host with standard Waffles
actions. Whiteboarding, navigation, and arranging media are library operations.
ECPresenter supplies the stage and an ambient-content callback. Applications can
reorder or override entries and replace the visual without copying activation
code. See [quick-menu setup and customization](waffles-implementation-guide.md#6-add-quick-menu-actions-and-whiteboarding).

## Implementation status and remaining work

The working branch is `af/waffle-glass`. It now includes current `main`, with
TouchEngine enabled. The separate TouchEngine rewrite remains in
[PR #49](https://github.com/Unispace365/DsQt/pull/49). ECPresenter's integration is
committed locally; verify branch availability before following GitHub links to
new work. The [backlog](waffles-backlog.md) records completed work separately
from open delivery requirements.

`DsContentLauncherModel` is exported by `Dsqt.Bridge`; applications import it
directly. Standard window-control icons are bundled with Waffles; legacy `qml/ux`
wrappers still use deployed app window icons. Presentation
pause suspends video and animated-image playback and the ambient slide timer,
while manual navigation and custom-layout hotspots remain available.

Core's media viewer now reports loading, empty, ready, and failed states. Waffles
presents those states consistently in media windows and slide layouts, with a
retry action for failed loads. The launcher explains empty folders, and missing
layout frames are visible. ECPresenter reports unavailable links and empty
presentations while preserving the existing presentation.

ECPresenter and ProjectCloner now use the same `bridgesync.toml` filename as
their settings lists. Their build configuration imports the optional Touch
controls and initializes WebEngine when available in the installed library.
The deployment validator checks the selected Qt kit, runtime modules, settings
files, and application icons. Target Bridge/platform selection is configuration
work: a platform missing from the selected database indicates a configuration
mismatch or the wrong Bridge environment, not a reason to change the schema.

Additional slide templates and the proposed new application demonstrating a
combined schema are on hold. Native schema adoption, hosted CI, and physical
device acceptance remain open. See the backlog for validation results.

Existing behavior-test records do not establish final Figma fidelity or hardware
acceptance. Test the whole application on the intended touchscreen, including
graphics acceleration, multiple touches, keyboard focus, live inputs, and idle
cleanup. Diagnostics and optional rendering/architecture extensions remain
future scope.

## Where to go next

- [Consolidated implementation guide](waffles-implementation-guide.md): prerequisites, application wiring, settings, and validation.
- [Source Tour](waffles-tour/index.html): detailed component walkthroughs and recipes; open the HTML locally for the styled tour.
- [Whiteboard reference](articles/whiteboard.md): drawing API, custom controls, and export behavior.
- [Figma design](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD) and [quick-menu motion](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=454-1302&m=dev).
- [DsQt on GitHub](https://github.com/Unispace365/DsQt) and [ECPresenter on GitHub](https://github.com/Unispace365/ECPresenter).
