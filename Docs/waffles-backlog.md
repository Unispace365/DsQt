# Waffles backlog

Running list of smaller / deferred items so we can keep pushing major functionality
without losing track. Candidate for Jira import later.

Convention: `- [ ] **Title** (area) — one-line note.`
Check the box when done; move long-finished items to the bottom or delete.

## Features

- [ ] **Loop via metadata** (media) — `media.loop` → set `loops: Infinite` at viewer open.
      The remaining half of FS Phase 4; hold-last-frame is already done via
      `endOfStreamPolicy: VideoOutput.KeepLastFrame`.
- [ ] **On-screen keyboard** (controls/web) — the web controls' keyboard button is currently
      a stub; needs an on-screen keyboard surface.
- [ ] **Per-type fullscreen controllers** (fullscreen) — `DsWaffleStage._showController` notes
      "could vary by `v.viewerType`"; only `fullscreenController` is wired. A
      `presentationController` component exists but isn't used yet.
- [ ] **Glass on non-media viewers** (glass) — `DsViewer` carries the glass config but only
      `DsTitledMediaViewer` renders the glass surface (menu / launcher / other viewers don't).

## Polish / UX

- [ ] **Resize the fullscreen controller to the Figma spec** (fullscreen/UX) — the controller is far
      bigger than the design. Figma is 4K, 1:1 with the stage. File:
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

- [x] **`modalLayer` for scrim + fullscreen** (stage/architecture) — done. Superseded by the
      named-layer refactor: `DsWaffleStage` now has six layers (background / presentation /
      viewer / modal1 / modal2 / modal3). Launcher lives in `modal1` (configurable via
      `launcherLayer`); scrim + fullscreen viewer + `FullscreenController` live in `modal2`,
      with `setFullscreen` reparenting the viewer into `modal2` on enter and back to its
      original layer on exit. Glass chain is now cross-layer.
- [ ] **Stale TEMP comment** (cleanup) — `DsTitledMediaViewer.qml`:
      `visible: true // TEMP: media hidden to inspect the glass background`. The media is shown
      now; the comment is misleading. Remove it.
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

- [ ] **`blurMultiplier` glass token** (theme) — expose one if a design ever needs a blur radius
      beyond what `glassBlurMax 64` gives (it extends radius cheaply, at some quality loss).
      Noted in the glass-and-theming tour page.

---

## Done

_(move completed items here, or delete)_
