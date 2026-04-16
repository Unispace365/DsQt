# TouchFilter Guide

`TouchFilter` is a Qt 6 event filter, provided by the `Dsqt::Touch` module, that
intercepts `QTouchEvent` objects at the `QQuickWindow` level and suppresses unwanted
touches before Qt's pointer-dispatch system delivers them to any handler in the scene.

Because it operates on raw `QEvent` objects, a filtered touch is invisible to
`PinchHandler`, `TapHandler`, `DragHandler`, `MultiPointTouchArea`, and every other
Qt Quick handler — not just to code that listens to `TouchFilter`'s own signals.

A working demonstration is in `Examples/TouchFilter`.

---

## Table of Contents

1. [Adding the Module](#adding-the-module)
2. [Installing the Filter in QML](#installing-the-filter-in-qml)
3. [Filtering Stages](#filtering-stages)
4. [Listening to Results](#listening-to-results)
5. [Configuration via TOML Settings](#configuration-via-toml-settings)
6. [Properties Reference](#properties-reference)
7. [Signals Reference](#signals-reference)
8. [Platform Notes](#platform-notes)
9. [Known Limitations](#known-limitations)

---

## Adding the Module

### CMakeLists.txt

```cmake
find_package(Dsqt REQUIRED COMPONENTS Core Touch)

target_link_libraries(myApp PRIVATE Dsqt::Core Dsqt::Touch)

qt_add_qml_module(myApp
    URI MyApp
    VERSION 1.0
    QML_FILES Main.qml
    IMPORT_PATH "${DSQT_QML_IMPORT_PATH}"
    DEPENDENCIES Dsqt::Core Dsqt::Touch QtQuick
)
```

### main.cpp — static plugin registration

```cpp
#ifdef DSQT_HAS_Core
Q_IMPORT_QML_PLUGIN(Dsqt_CorePlugin)
#endif
#ifdef DSQT_HAS_Touch
Q_IMPORT_QML_PLUGIN(Dsqt_TouchPlugin)
#endif
```

Add `Touch` to the `foreach` loop that sets `DSQT_HAS_*` compile definitions:

```cmake
foreach(_comp Core Touch)
    if(Dsqt_${_comp}_FOUND)
        target_compile_definitions(myApp PRIVATE DSQT_HAS_${_comp})
    endif()
endforeach()
```

---

## Installing the Filter in QML

```qml
import QtQuick
import Dsqt.Core
import Dsqt.Touch

Item {
    id: root

    TouchFilter {
        id: filter
        transientThresholdMs: 80
        smoothingFactor:      0.30
        proximityFilterPx:    40
    }

    // Install/reinstall whenever the enclosing window changes.
    onWindowChanged:       if (window) filter.window = window
    Component.onCompleted: if (window) filter.window = window
}
```

Setting `window` calls `installEventFilter` / `removeEventFilter` automatically.
No `MultiPointTouchArea` or input area is needed — the filter works at the window level.

---

## Filtering Stages

Four stages are applied to every new press, in order:

### 1 — Proximity rejection *(event-level)*

If `proximityFilterPx > 0`, a new press that lands within that pixel radius of any
active touch is **immediately suppressed**. The event never reaches the scene.

Set `proximityFilterPx` to `0` (the default) to disable.

### 2 — Transient suppression *(event-level)*

Every press is **buffered** for `transientThresholdMs` milliseconds.

- If the finger **lifts before the timer fires**: the buffered events are discarded.
  The scene never saw the press, so no gesture recogniser was started.
- If the **timer fires** while the finger is still down: the buffered press and moves
  are **re-injected** via `QCoreApplication::sendEvent` and delivered to the scene as
  normal Qt events.

This adds `transientThresholdMs` of latency to the start of every confirmed touch.
Typical values (60–120 ms) are imperceptible for drag gestures.

### 3 — Jitter smoothing *(signal-level only)*

Accepted move events **pass through unchanged**. Smoothed positions are computed via
an exponential moving average (`α = smoothingFactor`) and reported in the
`touchAccepted` signal. Qt handlers receive the raw hardware position; code that reads
`smoothX`/`smoothY` from `touchAccepted` gets the filtered position.

### 4 — Lift-resume bridging *(signal-level only)*

When an active touch lifts, the filter opens a `liftResumeThresholdMs` window. If a
new press arrives within `liftResumeDistancePx` during that window, the two touches
are treated as one continuous drag in the signal stream.

The underlying Qt events pass through unchanged for both the lift and the resume press.
Full event-level suppression would require remapping touch IDs inside `QTouchEvent`,
which is not possible without Qt private headers.

---

## Listening to Results

```qml
TouchFilter {
    id: filter

    onTouchAccepted: function(id, rawX, rawY, smoothX, smoothY, state) {
        if (state === 0) console.log("confirmed press at", smoothX, smoothY)
        if (state === 1) myItem.x = smoothX   // use smoothed position for moves
        if (state === 2) console.log("confirmed release")
    }

    onTouchFiltered: function(id, x, y, state, reason) {
        // reason: "transient" | "proximity" | "liftResume"
        console.log("suppressed:", reason)
    }

    onTouchReclassified: function(id, wasFiltered, reason) {
        // Fires once a pending touch is conclusively classified.
        // Use this to retroactively repaint a "pending" trail green or red.
    }
}
```

`state` values: `0` = pressed, `1` = moved, `2` = released.

### Preventing transient touches from triggering pinch

Because the filter operates at the event level, `PinchHandler` never sees a transient
touch. If you also want to prevent accepted touches from starting a pinch until two
*confirmed* touches exist, gate on the filter's accepted set:

```qml
property var acceptedLive: ({})

TouchFilter {
    onTouchAccepted: function(id, rawX, rawY, smoothX, smoothY, state) {
        if (state === 0) {
            acceptedLive[id] = { x: rawX, y: rawY }
            if (Object.keys(acceptedLive).length === 2)
                startPinch(acceptedLive)
        } else if (state === 1) {
            if (acceptedLive[id]) acceptedLive[id] = { x: rawX, y: rawY }
        } else if (state === 2) {
            delete acceptedLive[id]
        }
    }
}
```

---

## Configuration via TOML Settings

Add `touch_filter_settings.toml` to your `settings/` directory:

```toml
# settings/touch_filter_settings.toml
[touch_filter]
transient_ms    = 80      # Minimum touch duration before acceptance (ms)
smoothing_alpha = 0.30    # EMA alpha for jitter smoothing (0 = no movement, 1 = raw)
lift_resume_ms  = 120     # Lift-resume detection window (ms)
lift_resume_px  = 25      # Maximum distance for lift-resume match (px)
proximity_px    = 0       # Proximity-rejection radius (px, 0 = disabled)
```

Register it in `settings/engine.toml`:

```toml
[engine.extra]
app_settings = ["touch_filter_settings.toml"]
```

Load the values in QML via `DsSettingsProxy`:

```qml
import Dsqt.Core
import Dsqt.Touch

DsSettingsProxy {
    id:     fp
    target: "app_settings"
    prefix: "touch_filter"
}

TouchFilter {
    transientThresholdMs:  fp.getInt   ("transient_ms",    80)
    smoothingFactor:       fp.getDouble("smoothing_alpha", 0.30)
    liftResumeThresholdMs: fp.getInt   ("lift_resume_ms",  120)
    liftResumeDistancePx:  fp.getDouble("lift_resume_px",  25)
    proximityFilterPx:     fp.getInt   ("proximity_px",    0)
}
```

Use `Component.onCompleted` to break the binding if you want runtime slider overrides
to survive hot-reloads of the settings file.

---

## Properties Reference

| Property | Type | Default | Description |
|---|---|---|---|
| `window` | `QQuickWindow*` | `nullptr` | Target window. Setting this installs/uninstalls the event filter. |
| `transientThresholdMs` | `int` | `80` | Minimum touch duration (ms) before a press is confirmed. |
| `smoothingFactor` | `qreal` | `0.30` | EMA alpha for jitter smoothing. `0` = frozen, `1` = raw. |
| `liftResumeThresholdMs` | `int` | `120` | Window (ms) after a lift in which a nearby re-press is treated as a continuation. |
| `liftResumeDistancePx` | `qreal` | `25.0` | Maximum distance (px) for a lift-resume match. |
| `proximityFilterPx` | `qreal` | `0.0` | Rejection radius (px) around active touches. `0` disables proximity rejection. |
| `filterEnabled` | `bool` | `true` | When `false`, all events pass through and signals still fire (for visualisation). |

`reset()` is a `Q_INVOKABLE` slot that discards all in-flight state — useful when clearing a diagnostic UI.

---

## Signals Reference

```cpp
// Fired for every raw touch before any filtering decision.
// Use this to drive raw visualisation — it fires even for touches that will be suppressed.
// state: 0 = pressed, 1 = moved, 2 = released
void touchRaw(int id, qreal x, qreal y, int state);

// A touch passed all filters; its event has been (or will be) delivered to the scene.
// smoothX/Y carry the EMA-smoothed position; rawX/Y the original hardware position.
void touchAccepted(int id, qreal rawX, qreal rawY,
                   qreal smoothX, qreal smoothY, int state);

// A touch event was suppressed.
// reason: "transient" | "proximity" | "liftResume"
void touchFiltered(int id, qreal x, qreal y, int state, QString reason);

// Fired once a pending touch is conclusively classified (timer fired or finger lifted).
// wasFiltered = true  → rejected;  false → accepted (touchAccepted also fires)
void touchReclassified(int id, bool wasFiltered, QString reason);
```

All coordinates are in window/scene space.

---

## Platform Notes

### Windows — touch coordinate origin (LWE / WM_POINTER drivers)

On Windows with certain touch drivers (including the LWE / Large World Experience
stack used on some commercial displays), `QEventPoint::scenePosition()` returns
**global screen coordinates** rather than window-relative coordinates.

`TouchFilter` works around this by computing coordinates as:

```cpp
QPointF windowPos = pt.globalPosition() - m_window->position();
```

`globalPosition()` is always screen-absolute on all platforms, so subtracting the
window's screen position reliably produces window-relative coordinates regardless of
the driver.

### Qt 6.10 — `mapFromScene` not exposed to QML

`QQuickItem::mapFromScene(QPointF)` exists in C++ but is not marked `Q_INVOKABLE`
in Qt 6.10. Use `mapFromItem(null, x, y)` instead — passing `null` as the source item
maps from scene (window-root) coordinates, which is equivalent.

---

## Known Limitations

- **Lift-resume** operates at signal level only. The release and subsequent press
  events still reach Qt handlers unchanged. Full event suppression would require
  in-place mutation of `QTouchEvent` point lists, which is not possible without Qt
  private headers.
- **Re-injected events** (confirmed transient touches) contain only the single
  buffered point. In multi-touch sessions other active points appear as `Stationary`
  in the original events but are absent from re-injected ones. This is generally
  harmless for single-touch scenarios but may affect complex multi-touch gestures.
- **Jitter smoothing** does not modify the delivered event; it provides smoothed
  coordinates via signals only. Handlers that read `QEventPoint::position()` see
  the raw hardware position.
