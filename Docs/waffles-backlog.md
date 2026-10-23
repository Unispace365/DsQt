# Waffles backlog

Reconciled against DsQt `af/waffle-glass` and the local ECPresenter integration on
October 12, 2026, with the pre-PR follow-up below updated October 15.
Completed code is distinct from application integration and
device acceptance. See the [overview](waffles-overview.md) and
[implementation guide](waffles-implementation-guide.md).

## Pre-PR follow-up — October 14–15, 2026

These numbers match the current review list in the Waffles development chat;
the older application-planning item numbers below are a separate list.

| # | Task | Current status and evidence |
| --- | --- | --- |
| 1 | Viewer lookup and selection | Implemented and committed. |
| 2 | Consume ambient wake gestures | Implemented and committed, including disabling the shield's hover handler while inactive. |
| 3 | Explicit PDF coverage in CI | Implemented and committed. CI requires PDF; local kits may skip only PDF-dependent cases. |
| 4 | Canceled quick-menu gestures must not execute actions | Fixed locally. Cancellation is signaled separately from release; highlighted-wedge cancellation and reopening pass for five-finger and simulated mouse gestures. |
| 5 | Ambient black background stacking | Fixed locally. Its explicit z-order is directly beneath the ambient layer and above foreground content; it is hidden when ambient closes. |
| 6 | Custom menu lifecycle when a host detaches | Fixed locally. Host cleanup does not depend on delayed visual close signals; reattachment, reopening, and action selection pass. |
| 7 | Fullscreen scrim input | Fixed locally. The scrim consumes wheel and multitouch input without scrolling or zooming the PDF underneath. Pinching leaves fullscreen open; a single-finger tap still dismisses it. Scrolling reaches the underlying PDF again after the fullscreen viewer closes. All 51 focused input checks passed; physical-device acceptance remains part of #10. |
| 8 | Ambient audio policy | Implemented locally. Ambient suspends foreground video and animated images, including standard playlists and custom layouts, then restores prior playback state. Paused/stopped media stays paused/stopped. Ambient replacements retain suspension; new foreground media loads suspended. Custom templates must forward the separate `playbackSuspended` contract. |
| 9 | Backlog and deployment validation | Reconciled locally. PDF deployments now explicitly require the matching PDF QML plugin; current evidence is recorded below. |
| 10 | Hosted CI, live Bridge, and physical device acceptance | Open. Local checks do not establish these results. |
| 11 | Whole-display darkening during normal browsing | Not currently reproducible in ECPresenter, per the latest user testing. The ambient background fix may have resolved it, but the original cause remains unconfirmed. Recheck normal-browsing brightness during #10 acceptance. |
| 12 | PDF pinch minimum size | Fixed locally. Pinches cannot shrink the page below fit-to-frame size, preserving aspect ratio; subsequent zoom-in and existing locked/unlocked pinch behavior pass. |
| 13 | Locked PDF frame blocks input underneath | Existing implementation verified by new mouse/touch margin checks, including clicks, drags, and wheel events. No additional production input layer was needed. |

**Latest local validation:** the Qt 6.12 MSVC x64 Release playlist suite completed
in 172 seconds on October 15: **300 passed, zero failed, zero skipped**. The
fullscreen-scrim wheel/pinch regression and single-finger dismissal check pass;
all 51 focused panel input checks pass. All seven earlier regression cases and
the 15 audio-policy cases passed. The focused audio-policy
run passed 17 cases including setup/cleanup, covering prior playing/paused/stopped
state, animated frames, custom-layout background and tile media, ambient
replacement, newly loaded foreground media and playlists, preservation of the
playlist pause setting, a pause requested during suspension, and failed ambient
creation. The October 14 focused run without PDF passed 12 cases and correctly
skipped six PDF-dependent cases, with zero failures. The updated deployment
validator passed against the existing ECPresenter
Debug deployment, including the PDF plugin and all 58 SVG assets; missing
`content_settings.toml` and `menu.toml` remain optional-hook warnings. This is
file validation of that existing deployment, not a smoke test of the newly
changed framework. The older validation records below are historical evidence.

The content-state CTest suite was rebuilt against the shared media-viewer
changes and passed on October 15, including its loading, error, and recovery
checks. The matching WebEngine helper, runtime libraries, plugins, and resources
were checked before launch. Live Bridge and physical-device acceptance remain
open.

## Completed framework capabilities

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
| Shared assets | Waffles bundles Roboto, media controls, whiteboard controls, and all 14 standard window-control SVGs. Deploy app menu/launcher assets and the window icons still used by legacy `qml/ux` wrappers. |
| Build and exports | Qt 6.12 requirements, CI module selection, Cloner template presets, and direct Bridge adapter import are aligned. Current main is merged and TouchEngine is enabled. |
| Content state and recovery | Core provides media load/error status and retry. Waffles supplies the Figma-based `DsContentState` in media windows and slide templates, visible missing-frame feedback, and launcher loading/empty-folder states. |
| Deployment checks | The installed package exports its optional feature dependencies and capture include path. ProjectCloner/ECPresenter import optional Touch controls and initialize WebEngine. `Tools/ValidateWafflesDeployment.ps1` checks installed runtime files, settings names, and app SVG assets. |

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
- Visible feedback for unresolved links, removed linked slides, and empty presentations.
- `settings/bridgesync.toml` matches the filename in the engine settings list.

Earlier integration checks are recorded in
`ECPresenter/build/presentation-navigation-check/` (106 assertions). They cover
geometry, links, launcher selection, pointer input, cleanup, and idle return.
Those records are historical evidence, not fresh acceptance of the current branch.

## Open delivery work

| Task | Owner | Completion evidence |
| --- | --- | --- |
| Adopt the reusable schema | Application/schema maintainers | Preserved IDs/content, native Bridge validation, synchronized representative records, and matching application mappings; see the schema task below. |
| Additional slide templates — on hold (item 5) | DsQt Waffles / application | Implement required Gallery, Carousel, Cards, Agenda, or Collection renderers and register them when resumed. Schema types alone do not provide renderers. |
| New schema demonstration app — on hold (item 3) | Application/schema maintainers | When resumed, create with ProjectCloner and demonstrate Waffles plus application-specific schema features. |
| Target environment configuration (item 2) | Application | Correct the server, resource/cache paths and platform ID. If the expected platform is absent, configuration is wrong or the app is connected to the wrong Bridge; do not change the schema to fit that mismatch. |
| Installed application acceptance | Application | Run the deployment validator and startup checks with deployed imports; then confirm the intended Bridge/platform/cache and live synchronized content. |
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
| Library runtime tests | All nine CTest suites passed in Debug and Release. Final focused checks passed 19 content-state cases (including actual WebEngine failure and simulated missing capture input) and 25 playlist cases. No physical capture device was opened by the content-state tests. |
| Schema static checks against current DsQt | Passed: 13 types, four traits, 11 assets. |
| Schema regression suite | Passed: 35 tests. |
| ProjectCloner | Earlier fresh-consumer generation and Waffles smoke passed using the actual backend. For this follow-up, the updated ClonerSource template built/installed in an isolated Debug build and passed deployed file checks, including Touch and all 58 menu/launcher/window SVGs. The template's Bridge defaults still require application-specific configuration. |
| ECPresenter | Debug and Release build/install, deployed file checks, and ten-second startup smoke checks passed in fresh `WAFFLES_REVIEW` directories using deployed Qt/QML files. No missing Touch-module warning or component-load error was reported. The selected platform is absent from the current cache; live content acceptance remains configuration work. |
| Content feedback appearance | The rendered `DsContentState` snapshot was inspected against the Figma missing-content treatment. Full application/Figma and physical-display acceptance remain separate. |
| Hosted CI, native Bridge migration, physical touchscreen acceptance | Not run. |

Build/test logs are local artifacts under `Library/build/ninja/`; the new
content-state suite writes `Tests/ContentState/contentstate-results.xml` and
`missing-content.png` there. Template logs are under `Library/build/` and its
deployment is `Library/build/ClonerTemplateCheck/DEPLOY/Debug/`. ECPresenter logs
and fresh deployments are under `build/ninja-multi-6.12/` (`WAFFLES_REVIEW/`).

The earlier missing Touch-module deployment warning is resolved; Touch remains
separate from TouchEngine. ECPresenter's configured platform `UgDKTSLVVvv2` is
absent from its current cache. Correct the configuration or Bridge environment
before live content acceptance. Both app/template settings lists also retain
the legacy `content_settings.toml` and `menu.toml` hooks without local files;
the validator reports these as warnings for deployment-specific review.

The local Release build initially hit an assertion in Visual Studio's bundled
Ninja 1.13.2; switching the local build to Qt's Ninja 1.12.1 completed both
configurations. Hosted CI has not yet verified its own toolchain. Deployment
and hardware acceptance remain required even when automated behavior tests pass.
