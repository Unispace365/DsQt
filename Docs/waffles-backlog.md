# Waffles backlog

Running list of smaller / deferred items so we can keep pushing major functionality
without losing track. Candidate for Jira import later.

Convention: `- [ ] **Title** (area) — one-line note.`
Check the box when done; move long-finished items to the bottom or delete.

## Features

- [x] **Loop via metadata** (media) — done. `DsTitledMediaViewer` now reads `media.loop` from the
      bridge media descriptor and sets `DsMediaViewer.loops = MediaPlayer.Infinite` when truthy
      (else 0 = play once + KeepLastFrame). Finishes FS Phase 4.
- [x] **On-screen keyboard** (controls/web) — done (Phase 2c). `DsVirtualKeyboard` wraps Qt
      Virtual Keyboard's `InputPanel` and applies the bundled dark **`waffles`** style (in
      `Dsqt.Core`'s `keyboard.qrc`; selected via `QT_VIRTUALKEYBOARD_STYLE=waffles` per app +
      `qrc:/keyboard` added to the QML import path by `DsQmlApplicationEngine`). The launcher
      docks one above its close button in search mode; the web controls' keyboard button toggles a
      `DsFloatingKeyboard` (draggable, on the stage's `modal3`) via
      `DsWaffleStage.floatingKeyboardShown`. Multilingual locales come from
      `[waffles.keyboard] locales` (default Latin set; globe key cycles them). Only one InputPanel
      is active at a time (the docked panel gates off while the floating one is shown). Verify on
      device: web-input focus routing + whether the 1400 px floating keyboard feels right on 4K.
- [ ] **Per-type fullscreen controllers** (fullscreen) — `DsWaffleStage._showController` notes
      "could vary by `v.viewerType`"; only `fullscreenController` is wired. A
      `presentationController` component exists but isn't used yet.
- [x] **Glass on non-media viewers** (glass) — done. `DsContentLauncher` now renders glass via a
      `glassContext` consumed by its panel's `DsGlassBackground`; the `FullscreenController` does
      the same via `DsController.glassContext`. Both sample the cross-layer slot chain so their
      blur shows everything below in the layer stack. Any future non-media viewer can adopt the
      same pattern by exposing a glass-context QtObject.

## Polish / UX

- [x] **Resize the fullscreen controller to the Figma spec** (fullscreen/UX) — done. Frame is now
      459 × 99 (bar 444 × 99 + tab 30 × 30 overlap), title 16 px Roboto Thin (weight 100), bar
      corner radius 20, tab corner radius 8, icon buttons 24 × 24 with 16 px icons. The full
      Figma spec captured below stays for reference + the per-type pill specs (those still
      apply to `DsMediaControls` windowed and remain to-do).
      `Library/DsQt/Waffles/qml/ux/FullscreenController.qml`. Figma:
      [Image 83:3593](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-3593&m=dev),
      [Video 83:3594](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-3594&m=dev).

      Controller frame — **ALL types (image/video/pdf/web) are the SAME size**; the controls just fill the
      bottom row, so height does not change per type:
    - Frame **459 × 99** (W × H).
    - Bar: corner radius **20**, backdrop blur **10px**, Tonal-10 (#2E3439) border, tonal-0 @ 80% fill.
      Bar starts ~3.27% (~15px) in from the left so the collapse tab overlaps its left edge.
    - Collapse tab: **~30 × 30** square (Figma shows `aspect 50/50` = a 1:1 ratio, NOT 50px; actual width
      ≈ 6.54% of 459 ≈ 30px), radius **8**, Tonal-10 fill, drop shadow 0/4/4 rgba(0,0,0,.25); positioned
      left:0, top:34px → tab centre sits on the bar's left edge.
    - Title: **16px**, Helvetica Neue Thin → Roboto, white; top row, left-padded ~10% (~47px) to clear the tab.
    - Window buttons + media controls sit on the bottom row, laid out inline (not a nested pill).
    - Current values to replace: `barWidth 1400`, `barHeight 160`, `tabSize 56`, title `font.pixelSize 30`,
      radii from `DsTheme.glassRadius` (12). NOTE controller radius (20) and tab radius (8) are NOT `glassRadius`.
      (16px title / ~30px tab on a 4K touch UI reads small — worth a sanity-check on device.)

      Per-type control specs (the windowed `DsMediaControls` pills, bottom-inner; same controls are reused
      inline in the controller's bottom row). Each pill: own glass, radius **20**, blur **10px**, Tonal-10
      border; icon buttons **16 × 16**:
    - **Video** ([83:2785](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-2785&m=dev))
      — **356 × 37**: play/pause, scrubber (track Tonal-30 #5D6166, fill white, ellipse handle), loop
      (accent #00ADF7 when active), volume icon + volume slider.
    - **PDF** ([83:2888](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-2888&m=dev))
      — **236 × 36** (e.g. "1/6"); locked variant **250 × 36** (e.g. "02/89"): page label (12px Helvetica
      Neue Regular, +0.6px tracking), back, page slider (track white @ 25%, fill white), forward, lock/locked.
    - **Web** ([83:2974](https://www.figma.com/design/fjxsTDEKzJ7Xs50BQyptGE/White-Label-Waffles-DD?node-id=83-2974&m=dev))
      — **172 × 36**: keyboard, back, forward, refresh, lock/locked.

## Tech debt / cleanup

- [ ] **Bundle window icons in Waffles too** (waffles/assets) — `DsMediaControls.media()` now
      pulls glyphs from `qrc:/qt/qml/Dsqt/Waffles/data/images/waffles/media/...` (shipped with
      the module). Same treatment still owed to `DsMediaControls.win()` and
      `FullscreenController.windowIcon()`, both of which expand `%APP%/data/images/waffles/
      window/...` from app-side. ECPresenter and AssetViewer happen to ship that folder so
      things work today; a brand-new Waffles consumer would have missing icons. Same recipe as
      Roboto + the media set: copy `close / collapse / controller_collapse|expand / fullscreen
      / lock / locked` (+ `_pressed` variants) into `Library/DsQt/Waffles/data/images/waffles/
      window/`, list in `RESOURCES`, switch the helpers to `qrc:/...` paths.
- [ ] **Per-type DsMediaControls pill widths follow Figma in chrome:false too?** (decide)
      Today `pill.width = mc.width` when embedded in the FS controller (fills the row); the
      Figma's video/pdf/web pills (356/236/172) are only enforced in standalone chrome:true mode.
      The FS-controller inline row works fine filled, but it isn't a literal Figma match.



- [x] **`modalLayer` for scrim + fullscreen** (stage/architecture) — done. Superseded by the
      named-layer refactor: `DsWaffleStage` now has six layers (background / presentation /
      viewer / modal1 / modal2 / modal3). Launcher lives in `modal1` (configurable via
      `launcherLayer`); scrim + fullscreen viewer + `FullscreenController` live in `modal2`,
      with `setFullscreen` reparenting the viewer into `modal2` on enter and back to its
      original layer on exit. Glass chain is now cross-layer.
- [x] **Stale TEMP comment** (cleanup) — done. Removed the misleading
      `// TEMP: media hidden to inspect the glass background` annotation.
- [x] **Rename ECPresenter installer scripts** (ECPresenter/cleanup) — done. Deleted stale
      `ClonerSource.iss` / `ClonerSource_dev.iss` from both `ECPresenter/install/` and
      `Examples/AssetViewer/install/` (both had been copied from the ClonerSource template).
      `apphost.json` and `make_installer.bat` regenerate from `cmake/*.in` per project at
      configure time and were already project-named correctly; no template changes needed.

## Performance

- [ ] **C++ hybrid glass capture/compositing** (glass/perf) — replace the pure-QML live slot
      chain with a C++ implementation for performance. Watch the cost with several stacked
      viewers; that's the motivation.

## Docs

- [ ] **Tour: explain drillable vs openable in the launcher** (docs) — the launcher item shape
      now carries both `hasChildren` (UX hint: chevron + drill-on-tap, driven by
      `drillableKinds`) and `recordHasChildren` (data fact: bridge record actually has children,
      used by openable container viewers like presentations). Worth a short paragraph in the
      glass-and-theming tour page (or a new launcher tour page) once the search/keyboard work
      lands. Default `drillableKinds = ["folder", "playlist"]`; map "Presentation" / other
      always-open container types to a kind outside that list (e.g. `"presentation"`).


- [x] **`blurMultiplier` glass token** (theme) — done. `glassBlurMultiplier` (default 0.0) added
      to `DsTheme` + `[waffles.theme]` settings, plumbed through `DsWaffleStage` →
      `DsTitledMediaViewer` / `DsContentLauncher` / `DsController` glass contexts and into
      `DsGlassBackground`'s `MultiEffect`. Bump above 0 to extend blur radius beyond
      `glassBlurMax` without raising texture-lookup cost.

---

## Done

_(move completed items here, or delete)_
