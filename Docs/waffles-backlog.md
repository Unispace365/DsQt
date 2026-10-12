# Waffles backlog

Reconciled against DsQt `af/waffle-glass` and the local ECPresenter integration on
October 12, 2026. Completed code is distinct from application integration and
device acceptance. See the [overview](waffles-overview.md) and
[implementation guide](waffles-implementation-guide.md).

## Completed framework work

| Capability | Owner and implementation |
| --- | --- |
| Stage and media windows | DsQt Waffles: named layers, draggable/resizable viewers, fullscreen lifecycle, media controls, and cross-layer glass. |
| Content launcher | Waffles supplies browsing, breadcrumbs, loaded-tree search, filters, and keyboard UI. Bridge supplies the exported `DsContentLauncherModel`. |
| Configurable content contract | Bridge reads multiple type IDs, fields, browse/open policies, filters, playlist roles, timing, and renderer mappings from `waffles_content.toml` in `app_settings`. ProjectCloner supplies starter settings. |
| Presentations | Waffles: `DsPlaylistViewer`, ambient timing, manual navigation, cut/fade/slide transitions, component registry, and `PresentationController`. |
| Pause | Built-in templates pause video and animated images, including custom-layout backgrounds. Navigation and hotspots remain active; resume honors autoplay. Custom renderers implement the `paused` contract. |
| Custom layouts and hotspots | Bridge resolves frame/media/link data; Waffles renders positions, backgrounds, gestures, and hotspot signals. Explicit frame selection is strict; an empty setting enables aspect matching. |
| Quick menu | Core: Qt 6.12 Canvas2D circular rendering, reveal/selection animation, highlighting, selection signals, and timed closing. Application code supplies activation and actions. |
| Whiteboard | Waffles: Canvas Painter drawing, multi-touch pen/highlighter/eraser, colors/sizes, history, clear, backgrounds, and PNG export. |
| Shared assets | Waffles bundles Roboto, media controls, whiteboard controls, and all 14 standard window-control SVGs. Application menu/launcher assets still need deployment. |
| Build and exports | Qt 6.12 requirements, CI module selection, Cloner template presets, and direct Bridge adapter import are aligned. Current main is merged and TouchEngine is enabled. |

## Completed ECPresenter integration

ECPresenter owns application policy and is a reference for projects created by
ProjectCloner or existing DsQt applications. Its integration includes:

- Platform/content configuration, launcher requests, and destination routing.
- Startup ambient playback, idle return, and interactive presentation resume by
  slide UID after content reordering.
- Media/custom-layout renderer registration and explicit application frame selection.
- Five-touch/right-click quick-menu activation and nine application actions.
- Whiteboard opening and idle/ambient cleanup; normal Close retains the drawing.
- Direct import of the exported Bridge adapter, with no copied adapter source.

Earlier integration checks are recorded in
`ECPresenter/build/presentation-navigation-check/` (106 assertions). They cover
geometry, links, launcher selection, pointer input, cleanup, and idle return.
Those records are historical evidence, not fresh acceptance of the current branch.

## Open delivery work

| Task | Owner | Completion evidence |
| --- | --- | --- |
| Adopt the reusable schema | Application/schema maintainers | Preserved IDs/content, native Bridge validation, synchronized representative records, and matching application mappings; see the schema task below. |
| Content and load-error feedback | DsQt components / application | Visible unavailable-media and empty-content states, unresolved-destination feedback, and usable recovery paths. |
| Additional slide templates | DsQt Waffles / application | Implement required Gallery, Carousel, Cards, Agenda, or Collection renderers and register them. Schema types alone do not provide renderers. |
| Bridge Sync settings filename | ECPresenter | Resolve `engine.toml` referring to `bridgesync.toml` while the tracked file is `bridgesyc.toml`; verify the deployed setting. |
| Application deployment | Application | Ship menu/launcher assets and feature dependencies; verify installed startup, database/platform selection, and delayed/missing content. |
| Touchscreen and Figma acceptance | Application / design | Validate physical hit sizes, five-touch gestures, simultaneous drawing, menu motion, GPU/video appearance, web/keyboard focus, fullscreen, capture inputs, and repeated idle cleanup. |
| Hosted CI | DsQt | Run updated workflows on the branch. A local build does not establish a successful hosted run. The publishing workflow remains opt-in. |

## Design and optional follow-ups

- **Fullscreen control sizing:** verify the common 459 × 99 frame, 444 × 99 bar,
  30 × 30 collapse tab, 20 px bar radius, 24 px buttons, and 16 px glyphs against
  [image controls](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-3593&m=dev)
  and [video controls](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-3594&m=dev).
- **Inline control-pill widths:** design decision remains open for fullscreen
  `chrome:false` controls, which fill their row. Standalone targets are video
  356 × 37, PDF 236 × 36 (locked 250 × 36), and web 172 × 36. See
  [PDF controls](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-2888&m=dev)
  and [web controls](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-2974&m=dev).
- **Quick-menu acceptance:** compare current Canvas2D behavior with
  [the required UX](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=454-1302&m=dev)
  on the target display. Final motion/fidelity sign-off remains open.
- **Glass performance:** profile stacked viewers before deciding whether a C++
  capture/compositing implementation is warranted.
- **Source Tour maintenance:** align historical recipes with the consolidated
  guide, especially exported adapter imports, the `paused` template contract,
  and settings-driven browse/open behavior.
- **TouchEngine rewrite:** [PR #49](https://github.com/Unispace365/DsQt/pull/49)
  remains separate. This branch restores the current main implementation.

## Bridge schema integration

- [ ] **Define, package, and integrate the Waffles Bridge schema** (schema / Bridge / application)
      — establish the reusable content definitions Waffles needs, provide a versioned schema
      package, and integrate it into the current application schema.

The current reference application, ECPresenter, uses
[schema-downstreamdemo](https://github.com/Unispace365/schema-downstreamdemo).
The [DsQt adapter](../Library/DsQt/Bridge/qml/DsContentLauncherModel.qml) defaults to
that schema's field keys, type IDs, event names, and custom-layout frame IDs.
This task applies to applications using Bridge content.

**Package status, October 12, 2026:** the version 0.1.0 development candidate is
published in the private
[schema-waffles-dsqt repository](https://github.com/Unispace365/schema-waffles-dsqt)
on `main`, with local validation/settings updates at `a517890`. The published package starts at commit
[`9e2aead`](https://github.com/Unispace365/schema-waffles-dsqt/commit/9e2aeadf6d5a40897a4269d28244d4409253865c).
The checkout is at `C:\dev\schema-waffles-dsqt`. Its root contains the canonical
schema, Blueprint export, manifest, contract, adoption guide, and icons. The
package has 13 types and four traits; 35 regression tests and static checks
against the DsQt source pass. The source `schema-waffles` repository remains
separate. Application migration, native Bridge schema/content validation, and
device acceptance remain open.

| Deliverable | Scope |
| --- | --- |
| Schema contract | Inventory the types, traits (reusable field groups), field keys, value types, defaults, relationships, child ordering, and identifiers consumed by implemented Waffles features. Mark each as required for a feature, optional, or unsupported/planned. Cover media and folders, ambient/interactive playlists and slides, platform/event links, timing/transitions, custom-layout frames, and hotspots. Record configurable adapter mappings and fixed field assumptions separately. |
| Reusable package and repository | Published in the private `schema-waffles-dsqt` repository, including referenced traits/assets, import/export instructions, source revisions, compatibility mappings, and independent validation/comparison tools. Assign an individual maintainer and complete native validation before treating it as a deployment-ready dependency. |
| Current application integration | Compare the package with ECPresenter's `schema-downstreamdemo` schema; merge missing definitions and resolve differences while preserving existing content and relationships. Extend the relevant platform/event types with required traits or fields. Record any ID remapping and update the adapter, custom-layout frame configuration, and QML template registry accordingly. Configure the target Bridge environment and author representative content. |
| Validation and adoption guide | Verify CMS authoring through Bridge Sync to the running application. Document schema preparation for new applications created by ProjectCloner and integration into existing application schemas. Include sample content and the checks needed when upgrading the shared package. |

The package adds the optional `transition` selector, which still needs to be
merged into the application's schema. A read-only comparison found that the
older root `schema-waffles` definitions reuse two frame IDs with different
aspect ratios from current DsQt; the package follows current DsQt/demo geometry.
ECPresenter's `schema-downstreamdemo` frames match, but its remaining definition
differences still need review. Bridge Blueprint can regenerate IDs without
remapping every dependent reference, so integration must preserve IDs or migrate
all references explicitly. Additional schema types must be distinguished from
slide templates actually implemented in DsQt.

**Completion evidence:**

- A versioned package and contract are linked from the implementation guide,
  with an identified maintainer and compatible DsQt version.
- Both a fresh target schema and an existing application schema can adopt the
  selected features with valid references and explicit application mappings.
- ECPresenter demonstrates configured platform/event content, launcher browsing,
  media, ambient/interactive playlists, custom layouts, and hotspot navigation
  using the integrated schema; timing/transition behavior matches the contract.
- Existing application content and workflows remain valid after integration,
  and the schema changes, migration steps, and software/device checks are recorded.

See [schema preparation in the implementation guide](waffles-implementation-guide.md#prepare-the-application-schema)
and the [ownership overview](waffles-overview.md#bridge-schema-ownership).

## Validation record

| Check | Current local result |
| --- | --- |
| DsQt Debug and Release builds/install, Qt 6.12, TouchEngine enabled | Passed. Installed into the configured local DsQt prefix. |
| Library runtime tests | All eight CTest suites passed in both Debug and Release. The playlist suite passed all 25 cases, including media/GIF pause, resume, autoplay, navigation, hotspots, ambient timing, and bundled icons. |
| Schema static checks against current DsQt | Passed: 13 types, four traits, 11 assets. |
| Schema regression suite | Passed: 35 tests. |
| Fresh ProjectCloner consumer | Generated with the actual ProjectCloner backend; Debug build/install passed against the installed package with TouchEngine enabled. A Waffles smoke view instantiated the exported Bridge adapter, stage, and whiteboard and exited successfully. |
| ECPresenter | Debug and Release build/install passed. Deployed Debug startup loaded cached media without QML component-load errors. This does not establish live Bridge/schema acceptance. |
| Hosted CI, native Bridge migration, physical touchscreen acceptance | Not run. |

Build/test logs are local artifacts under `Library/build/ninja/`; consumer logs
are under `Library/build/ClonerWafflesSmoke/` and ECPresenter's
`build/ninja-multi-6.12/`. The fresh consumer had no content database, and
ECPresenter's configured platform ID was absent from its cached database. Both
reported the optional Touch input-filter module unavailable; this is separate
from TouchEngine. Validate the application's platform mapping and required
input modules during deployment acceptance.

The local Release build initially hit an assertion in Visual Studio's bundled
Ninja 1.13.2; switching the local build to Qt's Ninja 1.12.1 completed both
configurations. Hosted CI has not yet verified its own toolchain. Deployment
and hardware acceptance remain required even when automated behavior tests pass.
