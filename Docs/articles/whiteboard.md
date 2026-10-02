# Waffles whiteboard

The whiteboard belongs to the `Dsqt.Waffles` module. Applications can use the
default toolbar, replace it, or supply several control sets without reimplementing
drawing, history, or export.

## Components and dependencies

- `DsWhiteboardCanvas` is the C++ drawing engine ported from ContentLauncher.
  It uses Qt 6.12 CanvasPainter, supports mouse and simultaneous touch strokes,
  and retains CPU commands to recover drawing after resizing or renderer recreation.
- `DsWhiteboard` provides the public drawing/controller API and positions
  `DsControlSet` instances. It has no default toolbar or file dialog.
- `WhiteboardControls` provides the Figma toolbar using Waffles theme colors and
  bundled SVG icons. It calls the host through `signalObject`.
- `Whiteboard` combines the base board and default controls, a PNG save dialog,
  and save-error feedback.
- `DsWaffleStage` creates `Whiteboard` lazily and retains it when closed.

Link to `Dsqt::Waffles` and import `Dsqt.Waffles`. CanvasPainter is included in
the installed package dependencies; apps do not compile the drawing engine.

## Properties

All properties below belong to `DsWhiteboard` and are inherited by `Whiteboard`.
None is required.

| Property | Type | Default | Description |
|---|---|---|---|
| `shown` | bool | true | Shows the board and enables input. Hiding finishes active strokes. |
| `controls` | list<DsControlSet> | empty | Replaceable visual controls. `Whiteboard` supplies `WhiteboardControls`. |
| `model` | var | null | Optional application data passed to controls. |
| `config` | var | empty object | Optional visual configuration passed to controls. |
| `solidBackground` | bool | true | Solid background or transparent annotation mode. |
| `backgroundColor` | color | white | Board and PNG background when solid mode is enabled. |
| `tool` | DsWhiteboardTool.Tool | Pen | Pen, Brush, Highlighter, Eraser, or None for pointer input. Shape tools are intended for `addShape`. |
| `strokeWidth` | real | 6 | Positive drawing width in board coordinates. |
| `strokeColor` | color | black | Stroke color. Captured at the start of each stroke. |
| `pathCaching` | bool | true | Caches completed stroke geometry on the GPU. |
| `brushCenterAlpha` | real | 1 | Soft-brush center opacity, clamped to 0-1. |
| `brushFalloff` | real | 0.9 | Positive brush opacity falloff. |
| `brushMinLayers` | int | 4 | Minimum brush layers, clamped to 1-64. |
| `brushMaxLayers` | int | 12 | Maximum brush layers, clamped to 1-64. |
| `canUndo` | readonly bool | false | Undo is available and no pointer stroke is active. |
| `canRedo` | readonly bool | false | Redo is available and no pointer stroke is active. |
| `hasDrawing` | readonly bool | false | At least one drawing operation is applied, including eraser operations. Does not scan pixels. |
| `drawing` | readonly bool | false | One or more pointer strokes are active. |
| `saving` | readonly bool | false | An asynchronous PNG capture/save is in progress. |
| `drawingCanvas` | readonly DsWhiteboardCanvas | internal canvas | Advanced drawing/input access. |
| `captureItem` | readonly Item | drawing surface | Background and drawing without controls; suitable for glass sampling. |

`WhiteboardControls.strokeSizes` defaults to `[6, 18, 36]`, and its `colors`
property contains the eight Figma swatches. Both can be configured.
It uses `BottomInner` placement with a 69-unit bottom offset, scaled by
`DsTheme.uiScale`. The toolbar shrinks to fit narrower hosts.

## Commands and signals

`undo()` and `redo()` change the applied history; both are ignored during an
active stroke. A new stroke discards redo history. Up to 64 recent operations
can be undone; older drawing remains visible. Concurrent strokes are recorded
in pointer-start order.

`clear()` permanently clears drawing, history, and active strokes. `close()`
finishes strokes, sets `shown` to false, and emits `closeRequested()`; it retains
the drawing.

`requestSave()` finishes strokes and emits `saveRequested()`. The default
`Whiteboard` responds by opening a file picker. A custom host can instead choose
a path and call `save(file)` directly.

`save(file)` accepts a local `.png` URL and returns whether capture started.
It emits `saved(file)` after a successful write, or `saveFailed(file, message)`
on failure. The exported image includes the drawing and selected background.
It excludes the toolbar, clock, and underlying application content. Transparent
mode produces a transparent PNG, sized in board coordinates regardless of Windows
display scaling. Save requires the board to be visible and
attached to a window.

`addShape(tool, x, y, width, height, color, filled, strokeWidth, angle = 0)`
commits a shape as one undoable operation. Tools are Circle, Triangle, Square,
Line, Star, and Arrow. Dimensions and angles use board coordinates and degrees.
Line and Arrow retain directional start/end coordinates. Shape editing visuals
are not part of the default toolbar.

## Control contract

The host assigns itself to each control's `signalObject`, and supplies `model`
and `config`. Controls bind enabled/selected states to the host properties and
invoke its methods. Moving or replacing controls does not replace the canvas. Declarative assignments
to controls replace the default list rather than appending to it.

Use `implicitWidth` and `implicitHeight` for natural control size, and `edge`,
`offset`, `horizontalOffset`, and `verticalOffset` for placement. Top/bottom
controls are horizontally centered; left/right controls are vertically centered.
Both inner and outer edges are placed inside the full-stage whiteboard bounds.
Center and CenterBack use the center position. Controls remain visible while the
board is shown.

## Stage integration

`stage.whiteboard` is the replaceable Component, defaulting to `Whiteboard`.
Custom components must derive from `DsWhiteboard`.

`openWhiteboard()` creates or reopens the board and returns its instance.
`activeWhiteboard` is the retained instance; `whiteboardShown` tracks visibility.
`closeWhiteboard()` hides and preserves it.
`closeWhiteboard(true)` clears and hides it.
`clearForeground()` also clears and hides the board.

ECPresenter opens the board from its quick menu and clears it on idle or an
explicit switch to ambient mode. Normal toolbar Close preserves the drawing.

## Usage example

Default board:

```qml
import QtQuick
import Dsqt.Waffles

DsWaffleStage {
    id: stage
    // Call stage.openWhiteboard() from the application's menu.
}
```

A replacement visual control, retaining the default save dialog and drawing API:

```qml
import QtQuick
import QtQuick.Controls
import Dsqt.Waffles

Whiteboard {
    controls: [
        DsControlSet {
            id: bar
            edge: DsControlSet.BottomInner
            offset: 32
            implicitWidth: 240
            implicitHeight: 48

            Row {
                Button {
                    text: qsTr("Undo")
                    enabled: bar.signalObject?.canUndo ?? false
                    onClicked: bar.signalObject.undo()
                }
                Button {
                    text: qsTr("Save")
                    onClicked: bar.signalObject.requestSave()
                }
                Button {
                    text: qsTr("Close")
                    onClicked: bar.signalObject.close()
                }
            }
        }
    ]
}
```

Drawing is retained only for the lifetime of the component. PNG export is a
raster image, not an editable project format. The command history grows during a
session and is released by `clear()` or component destruction.
