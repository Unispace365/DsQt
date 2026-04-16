import QtQuick
import QtQuick.Controls
import Dsqt.Core
import Dsqt.Touch

/// Touch diagnostic view.
///
/// Layout:
///   Left        – filter control panel (sliders + stats + toggles + clear)
///   Right-top   – trail canvas: shows raw/smoothed/pending/filtered paths
///   Right-bottom – three gesture demo boxes: pinch/zoom, double-tap, long-press
///
/// Colour key:
///   Green  solid  – accepted raw path
///   Blue   dashed – accepted smoothed path (EMA)
///   Yellow        – pending (waiting for transient timer)
///   Red           – filtered  (transient / proximity / lift-resume)
///
/// Each track is labelled ↓ DOWN / → DRAG / ↑ UP with a reason note when filtered.
/// Trails persist until Clear is pressed.
Item {
    id: root

    // ── Settings ──────────────────────────────────────────────────────────────
    DsSettingsProxy {
        id:     filterProxy
        target: "app_settings"
        prefix: "touch_filter"
    }

    // ── Reusable large-touch slider control ───────────────────────────────────
    component CtrlSlider: Item {
        id: cs
        property string  label:    ""
        property string  suffix:   ""
        property real    from:     0
        property real    to:       100
        property alias   value:    sl.value
        property real    stepSize: 1
        property int     decimals: 0

        implicitHeight: ctrlCol.implicitHeight

        Column {
            id:      ctrlCol
            anchors { left: parent.left; right: parent.right; top: parent.top }
            spacing: 2

            Row {
                width: parent.width
                Text {
                    text:            cs.label
                    color:           "#9aaabf"
                    font.pixelSize: 16
                    width:           parent.width - valLabel.implicitWidth - 4
                    elide:           Text.ElideRight
                }
                Text {
                    id:             valLabel
                    text:           cs.decimals > 0
                                    ? sl.value.toFixed(cs.decimals) + cs.suffix
                                    : Math.round(sl.value) + cs.suffix
                    color:          "#dde8ff"
                    font.pixelSize: 16
                    font.bold:      true
                }
            }

            Slider {
                id:       sl
                width:    parent.width
                height:   32
                from:     cs.from
                to:       cs.to
                stepSize: cs.stepSize
                value:    cs.value

                background: Rectangle {
                    x:      sl.leftPadding
                    y:      sl.topPadding + sl.availableHeight / 2 - height / 2
                    width:  sl.availableWidth
                    height: 5
                    radius: 3
                    color:  "#1e2a3a"
                    Rectangle {
                        width:  sl.visualPosition * parent.width
                        height: parent.height
                        radius: parent.radius
                        color:  "#3d72c8"
                    }
                }

                handle: Rectangle {
                    x:      sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
                    y:      sl.topPadding  + sl.availableHeight / 2 - height / 2
                    width:  28
                    height: 28
                    radius: 14
                    color:  sl.pressed ? "#5a8ae0" : "#3d72c8"
                    border { color: "#7aaaee"; width: 2 }
                }
            }
        }
    }

    // ── Filter ────────────────────────────────────────────────────────────────
    TouchFilter {
        id: filter
        transientThresholdMs:  ctrlTransient.value
        smoothingFactor:       ctrlSmooth.value
        liftResumeThresholdMs: ctrlLift.value
        liftResumeDistancePx:  ctrlLiftDist.value
        proximityFilterPx:     ctrlProximity.value
        filterEnabled:         ctrlEnabled.filterOn

        onTouchRaw: function(id, x, y, state) {
            var local = canvasArea.mapFromItem(null, x, y)
            canvasArea._live[id] = { x: local.x, y: local.y }
            if      (state === 0) root.recordPress  (id, local.x, local.y)
            else if (state === 1) root.recordMove   (id, local.x, local.y)
            else                  root.recordRelease(id, local.x, local.y)
        }

        onTouchAccepted: function(id, rawX, rawY, smoothX, smoothY, state) {
            var t = touchTracks[id]
            if (t) {
                var sl = canvasArea.mapFromItem(null, smoothX, smoothY)
                if (state === 0)
                    t.smoothTrail = [{ x: sl.x, y: sl.y }]
                else
                    t.smoothTrail.push({ x: sl.x, y: sl.y })
            }
            canvas.requestPaint()
        }

        onTouchFiltered: function(id, x, y, state, reason) {
            var t = touchTracks[id]
            if (t) t.filterReason = reason
            canvas.requestPaint()
        }

        onTouchReclassified: function(id, wasFiltered, reason) {
            var t = touchTracks[id]
            if (!t) return
            var wasPending = t.classification === 'pending'
            t.classification = wasFiltered ? 'filtered' : 'accepted'
            t.filterReason   = t.filterReason || reason
            if (wasPending) {
                pendingCount = Math.max(0, pendingCount - 1)
                if (wasFiltered) filteredCount++
                else             acceptedCount++
            }
            canvas.requestPaint()
        }
    }

    onWindowChanged: function(w) { if (w) filter.window = w }
    Component.onCompleted: { var w = Window.window; if (w) filter.window = w }

    // ── Touch track state ─────────────────────────────────────────────────────
    property var  touchTracks:  ({})
    property int  pendingCount:  0
    property int  acceptedCount: 0
    property int  filteredCount: 0

    property bool showSmoothed: true

    // ── Track helpers ─────────────────────────────────────────────────────────
    function recordPress(id, sx, sy) {
        touchTracks[id] = {
            id:             id,
            rawTrail:       [{ x: sx, y: sy }],
            smoothTrail:    [],
            state:          'down',
            classification: 'pending',
            filterReason:   '',
            active:         true
        }
        pendingCount++
        canvas.requestPaint()
    }

    function recordMove(id, sx, sy) {
        var t = touchTracks[id]
        if (!t) return
        t.rawTrail.push({ x: sx, y: sy })
        t.state = 'drag'
        canvas.requestPaint()
    }

    function recordRelease(id, sx, sy) {
        var t = touchTracks[id]
        if (!t) return
        t.rawTrail.push({ x: sx, y: sy })
        t.state  = 'up'
        t.active = false
        canvas.requestPaint()
    }

    function clearAll() {
        touchTracks   = {}
        pendingCount  = 0
        acceptedCount = 0
        filteredCount = 0
        canvasArea._live = {}
        filter.reset()
        canvas.requestPaint()
    }

    // ── Layout ────────────────────────────────────────────────────────────────
    Rectangle { anchors.fill: parent; color: "#080e18" }

    Row {
        anchors.fill: parent

        // ── Controls panel ────────────────────────────────────────────────────
        Rectangle {
            id:     controlsPanel
            width:  260
            height: parent.height
            color:  "#0d1520"

            Column {
                anchors { fill: parent; margins: 12 }
                spacing: 6

                Text {
                    text:           "TOUCH FILTER"
                    color:          "#4a6a9a"
                    font.pixelSize: 13
                    font.bold:      true
                    font.letterSpacing: 3
                }

                // Private-reinject status badge (compile-time constant)
                Rectangle {
                    width: parent.width; height: 22; radius: 3
                    color:  filter.privateReinject ? "#0e2010" : "#1c1408"
                    border { color: filter.privateReinject ? "#2a6a30" : "#5a4010"; width: 1 }
                    Row {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 7 }
                        spacing: 6
                        Rectangle {
                            width: 6; height: 6; radius: 3
                            color: filter.privateReinject ? "#28D769" : "#c8a828"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text:  filter.privateReinject ? "Private reinject ON" : "Private reinject OFF"
                            color: filter.privateReinject ? "#50aa60"              : "#aa8830"
                            font.pixelSize: 10
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // Stats
                Rectangle {
                    width:  parent.width
                    height: 36
                    radius: 4
                    color:  "#111c2e"
                    Row {
                        anchors.centerIn: parent
                        spacing: 10
                        Repeater {
                            model: [
                                { label: "PASS",   value: root.acceptedCount, color: "#28D769" },
                                { label: "WAIT",   value: root.pendingCount,  color: "#FFBE00" },
                                { label: "FILTER", value: root.filteredCount, color: "#FF503C" }
                            ]
                            Column {
                                spacing: 1
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text:           modelData.value
                                    color:          modelData.color
                                    font.pixelSize: 16
                                    font.bold:      true
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text:           modelData.label
                                    color:          Qt.darker(modelData.color, 1.4)
                                    font.pixelSize: 9
                                    font.letterSpacing: 1.5
                                }
                            }
                        }
                    }
                }

                // Sliders
                CtrlSlider { id: ctrlTransient; width: parent.width; label: "Transient";  suffix: " ms"; from: 10;  to: 500;  value: 80;   stepSize: 5;    Component.onCompleted: value = filterProxy.getInt("transient_ms", 80) }
                CtrlSlider { id: ctrlSmooth;    width: parent.width; label: "Smoothing α"; suffix: "";   from: 0.0; to: 1.0;  value: 0.30; stepSize: 0.01; decimals: 2; Component.onCompleted: value = filterProxy.getFloat("smoothing_alpha", 0.30) }
                CtrlSlider { id: ctrlLift;      width: parent.width; label: "Lift resume"; suffix: " ms"; from: 20;  to: 400;  value: 120;  stepSize: 5;    Component.onCompleted: value = filterProxy.getInt("lift_resume_ms", 120) }
                CtrlSlider { id: ctrlLiftDist;  width: parent.width; label: "Lift dist";   suffix: " px"; from: 0;   to: 200;  value: 25;   stepSize: 5;    Component.onCompleted: value = filterProxy.getFloat("lift_resume_px", 25) }
                CtrlSlider { id: ctrlProximity; width: parent.width; label: "Proximity";   suffix: " px"; from: 0;   to: 200;  value: 0;    stepSize: 5;    Component.onCompleted: value = filterProxy.getInt("proximity_px", 0) }

                // Toggle: filter enabled
                Rectangle {
                    id:     ctrlEnabled
                    property bool filterOn: true
                    width:  parent.width; height: 32; radius: 4
                    color:  filterOn ? "#1a2d1a" : "#2d1a1a"
                    border { color: filterOn ? "#3d883d" : "#883d3d"; width: 2 }
                    Row {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
                        spacing: 7
                        Rectangle { width: 8; height: 8; radius: 4; color: ctrlEnabled.filterOn ? "#28D769" : "#FF503C"; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: ctrlEnabled.filterOn ? "Filter enabled" : "Filter disabled"; color: ctrlEnabled.filterOn ? "#dde8ff" : "#5a6a7a"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                    }
                    TapHandler { onTapped: ctrlEnabled.filterOn = !ctrlEnabled.filterOn }
                }

                // Toggle: show smoothed trail
                Rectangle {
                    width:  parent.width; height: 32; radius: 4
                    color:  root.showSmoothed ? "#1a2d4a" : "#111c2e"
                    border { color: root.showSmoothed ? "#3d72c8" : "#1e2a3a"; width: 2 }
                    Row {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
                        spacing: 7
                        Rectangle { width: 8; height: 2; radius: 1; color: "#50A0FF"; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: "Show smoothed trail"; color: root.showSmoothed ? "#dde8ff" : "#5a6a7a"; font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter }
                    }
                    TapHandler { onTapped: root.showSmoothed = !root.showSmoothed }
                }

                Item { width: 1; height: 2 }

                Rectangle {
                    width: parent.width; height: 36; radius: 4
                    color: clearHover.containsMouse ? "#2a1520" : "#1a0d14"
                    border { color: "#7a2030"; width: 2 }
                    Text { anchors.centerIn: parent; text: "CLEAR"; color: "#ee4455"; font.pixelSize: 15; font.bold: true; font.letterSpacing: 3 }
                    HoverHandler { id: clearHover }
                    TapHandler  { onTapped: root.clearAll() }
                }
            }
        }

        // ── Right side: canvas + demo strip ──────────────────────────────────
        Item {
            id:     rightPanel
            width:  parent.width - controlsPanel.width
            height: parent.height

            // ── Gesture demo strip (bottom) ───────────────────────────────────
            // Six boxes: Pinch | Single·TH | Double·TH | Double·MPTA | Hold·TH | Hold·MPTA
            // TH  = TapHandler  (amber label) — may require DSQT_TOUCH_PRIVATE_REINJECT
            // MPTA = MultiPointTouchArea (teal label) — always reliable
            Row {
                id:     demoStrip
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: Math.max(150, Math.round(parent.height * 0.30))

                readonly property int boxW: Math.floor(width / 6)

                // ── Shared helper components ──────────────────────────────────
                // Inline reusable arc-paint function defined per-canvas below.

                // ── 1. Pinch / Zoom — PinchHandler ───────────────────────────
                Rectangle {
                    id:     pinchBox
                    width:  demoStrip.boxW
                    height: parent.height
                    color:  "#060d18"; clip: true
                    border { color: "#1a2a3a"; width: 1 }

                    property real contentScale: 1.0
                    property real panX: 0.0
                    property real panY: 0.0

                    Column {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 6 }
                        spacing: 1
                        Text { text: "PINCH / ZOOM";  color: "#3a5a7a"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: "PinchHandler";  color: "#4a7a4a"; font.pixelSize: 9; anchors.horizontalCenter: parent.horizontalCenter }
                    }

                    Item {
                        anchors.fill: parent
                        transform: [
                            Translate { x: pinchBox.panX; y: pinchBox.panY },
                            Scale { xScale: pinchBox.contentScale; yScale: pinchBox.contentScale; origin.x: pinchBox.width / 2; origin.y: pinchBox.height / 2 }
                        ]
                        Repeater {
                            model: 25
                            Rectangle {
                                x: pinchBox.width  / 2 - 60 + (index % 5) * 30
                                y: pinchBox.height / 2 - 60 + Math.floor(index / 5) * 30
                                width: 6; height: 6; radius: 3; color: "#2a6aaa"
                            }
                        }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 6 }
                        text: pinchBox.contentScale.toFixed(2) + "×"; color: "#2a4a6a"; font.pixelSize: 10
                    }

                    PinchHandler {
                        target: null; grabPermissions: PointerHandler.CanTakeOverFromAnything
                        property bool skipFirst: false
                        onActiveChanged: { if (active) skipFirst = true }
                        onScaleChanged: (delta) => { if (active) pinchBox.contentScale = Math.max(0.15, Math.min(10.0, pinchBox.contentScale * delta)) }
                        onActiveTranslationChanged: (t) => { if (!active) return; if (skipFirst) { skipFirst = false; return }; pinchBox.panX += t.x; pinchBox.panY += t.y }
                    }
                    DragHandler {
                        target: null; acceptedDevices: PointerDevice.TouchScreen; acceptedButtons: Qt.NoButton; maximumPointCount: 1
                        grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                        property bool skipFirst: false
                        onActiveChanged: { if (active) skipFirst = true }
                        onActiveTranslationChanged: (t) => { if (!active) return; if (skipFirst) { skipFirst = false; return }; pinchBox.panX += t.x; pinchBox.panY += t.y }
                    }
                }

                // ── 2. Single Tap — TapHandler ────────────────────────────────
                Rectangle {
                    id:     singleTapTH
                    width:  demoStrip.boxW; height: parent.height
                    color:  "#0e0c06"
                    border { color: "#2a2810"; width: 1 }

                    property int tapCount: 0

                    Column {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 6 }
                        spacing: 1
                        Text { text: "SINGLE TAP"; color: "#3a5a7a"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: "TapHandler"; color: "#9a8a40"; font.pixelSize: 9; anchors.horizontalCenter: parent.horizontalCenter }
                    }

                    Rectangle {
                        id:               stDot
                        anchors.centerIn: parent
                        width: 22; height: 22; radius: 11; color: "#5a6030"
                        ScaleAnimator on scale { id: stAnim; from: 1.0; to: 1.8; duration: 180; easing.type: Easing.OutBack; running: false; onFinished: stDot.scale = 1.0 }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 6 }
                        text: singleTapTH.tapCount > 0 ? "× " + singleTapTH.tapCount : " "
                        color: "#8a7a30"; font.pixelSize: 12; font.bold: true
                    }

                    TapHandler {
                        acceptedButtons: Qt.NoButton
                        gesturePolicy:   TapHandler.DragThreshold
                        dragThreshold:   30
                        onTapped: { singleTapTH.tapCount++; stAnim.restart() }
                    }
                }

                // ── 3. Double Tap — TapHandler ────────────────────────────────
                Rectangle {
                    id:     doubleTapTH
                    width:  demoStrip.boxW; height: parent.height
                    color:  "#0e0c06"
                    border { color: "#2a2810"; width: 1 }

                    property int tapCount: 0

                    Column {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 6 }
                        spacing: 1
                        Text { text: "DOUBLE TAP"; color: "#3a5a7a"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: "TapHandler"; color: "#9a8a40"; font.pixelSize: 9; anchors.horizontalCenter: parent.horizontalCenter }
                    }

                    Item {
                        anchors.centerIn: parent; width: 56; height: 56
                        Rectangle {
                            id: dtTHRing; anchors.centerIn: parent
                            width: 50; height: 50; radius: 25; color: "transparent"
                            border { color: "#4a4020"; width: 2 }
                            ScaleAnimator on scale { id: dtTHRingAnim; from: 1.0; to: 1.6; duration: 250; easing.type: Easing.OutCubic; running: false; onFinished: dtTHRing.scale = 1.0 }
                        }
                        Rectangle {
                            id: dtTHDot; anchors.centerIn: parent
                            width: 18; height: 18; radius: 9; color: "#6a5a20"
                            ScaleAnimator on scale { id: dtTHDotAnim; from: 1.0; to: 2.0; duration: 200; easing.type: Easing.OutBack; running: false; onFinished: dtTHDot.scale = 1.0 }
                        }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 6 }
                        text: doubleTapTH.tapCount > 0 ? "× " + doubleTapTH.tapCount : " "
                        color: "#8a7a30"; font.pixelSize: 12; font.bold: true
                    }

                    TapHandler {
                        acceptedButtons: Qt.NoButton
                        // ReleaseWithinBounds: tap is valid as long as the release
                        // lands inside the item — no drag cancellation during the tap.
                        gesturePolicy:   TapHandler.ReleaseWithinBounds

                        // Qt's built-in tapCount uses mouseDoubleClickDistance (~4px),
                        // too tight for touch.  Track time + position ourselves with a
                        // 500ms / 40px window — standard touch double-tap tolerances.
                        property real  _lastTap: 0
                        property point _lastPos: Qt.point(0, 0)
                        onTapped: {
                            var now = Date.now()
                            var dx  = point.position.x - _lastPos.x
                            var dy  = point.position.y - _lastPos.y
                            var dist = Math.sqrt(dx*dx + dy*dy)
                            if (now - _lastTap < 500 && dist < 40) {
                                doubleTapTH.tapCount++
                                dtTHRingAnim.restart()
                                dtTHDotAnim.restart()
                                _lastTap = 0
                            } else {
                                _lastTap = now
                                _lastPos = Qt.point(point.position.x, point.position.y)
                            }
                        }
                    }
                }

                // ── 4. Double Tap — MultiPointTouchArea ───────────────────────
                Rectangle {
                    id:     doubleTapMPTA
                    width:  demoStrip.boxW; height: parent.height
                    color:  "#060e0e"
                    border { color: "#102828"; width: 1 }

                    property int tapCount: 0

                    Column {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 6 }
                        spacing: 1
                        Text { text: "DOUBLE TAP"; color: "#3a5a7a"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: "MultiPoint"; color: "#409a8a"; font.pixelSize: 9; anchors.horizontalCenter: parent.horizontalCenter }
                    }

                    Item {
                        anchors.centerIn: parent; width: 56; height: 56
                        Rectangle {
                            id: dtMPTARing; anchors.centerIn: parent
                            width: 50; height: 50; radius: 25; color: "transparent"
                            border { color: "#1e4a4a"; width: 2 }
                            ScaleAnimator on scale { id: dtMPTARingAnim; from: 1.0; to: 1.6; duration: 250; easing.type: Easing.OutCubic; running: false; onFinished: dtMPTARing.scale = 1.0 }
                        }
                        Rectangle {
                            id: dtMPTADot; anchors.centerIn: parent
                            width: 18; height: 18; radius: 9; color: "#206a6a"
                            ScaleAnimator on scale { id: dtMPTADotAnim; from: 1.0; to: 2.0; duration: 200; easing.type: Easing.OutBack; running: false; onFinished: dtMPTADot.scale = 1.0 }
                        }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 6 }
                        text: doubleTapMPTA.tapCount > 0 ? "× " + doubleTapMPTA.tapCount : " "
                        color: "#309a8a"; font.pixelSize: 12; font.bold: true
                    }

                    MultiPointTouchArea {
                        anchors.fill: parent; minimumTouchPoints: 1; maximumTouchPoints: 1
                        property real  _lastRelease: 0
                        property point _lastPos:     Qt.point(0, 0)
                        onReleased: (touchPoints) => {
                            var now = Date.now()
                            var tp  = touchPoints[0]
                            var dx  = tp.x - _lastPos.x
                            var dy  = tp.y - _lastPos.y
                            var dist = Math.sqrt(dx*dx + dy*dy)
                            if (now - _lastRelease < 500 && dist < 40) {
                                doubleTapMPTA.tapCount++; dtMPTARingAnim.restart(); dtMPTADotAnim.restart()
                                _lastRelease = 0
                            } else {
                                _lastRelease = now
                                _lastPos = Qt.point(tp.x, tp.y)
                            }
                        }
                    }
                }

                // ── 5. Long Press — TapHandler ────────────────────────────────
                Rectangle {
                    id:     holdTH
                    width:  demoStrip.boxW; height: parent.height
                    color:  "#0e0c06"
                    border { color: "#2a2810"; width: 1 }

                    property real holdProgress:  0.0
                    property bool holdTriggered: false

                    onHoldProgressChanged:  holdTHArc.requestPaint()
                    onHoldTriggeredChanged: holdTHArc.requestPaint()

                    Column {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 6 }
                        spacing: 1
                        Text { text: "HOLD"; color: "#3a5a7a"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: "TapHandler"; color: "#9a8a40"; font.pixelSize: 9; anchors.horizontalCenter: parent.horizontalCenter }
                    }

                    Canvas {
                        id: holdTHArc; anchors.centerIn: parent; width: 56; height: 56
                        onPaint: {
                            var ctx = getContext("2d"), cx = width/2, cy = height/2, r = 22
                            ctx.clearRect(0, 0, width, height)
                            ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI*2); ctx.strokeStyle = "#1a1808"; ctx.lineWidth = 5; ctx.stroke()
                            if (holdTH.holdProgress > 0) {
                                var s = -Math.PI/2
                                ctx.beginPath(); ctx.arc(cx, cy, r, s, s + holdTH.holdProgress * Math.PI * 2)
                                ctx.strokeStyle = holdTH.holdTriggered ? "#c8a828" : "#9a8030"; ctx.lineWidth = 5; ctx.stroke()
                            }
                            ctx.beginPath(); ctx.arc(cx, cy, 8, 0, Math.PI*2)
                            ctx.fillStyle = holdTH.holdTriggered ? "#c8a828" : holdTH.holdProgress > 0 ? "#9a8030" : "#1a1808"; ctx.fill()
                        }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 6 }
                        text: holdTH.holdTriggered ? "HELD!" : " "; color: "#c8a828"; font.pixelSize: 12; font.bold: true
                    }

                    TapHandler {
                        id:                 holdTHHandler
                        acceptedButtons:    Qt.NoButton
                        longPressThreshold: 0.6
                        onLongPressed:      holdTH.holdTriggered = true
                        onPressedChanged: {
                            // pressed goes false on physical release regardless of grab type.
                            // Only reset fill progress here — not holdTriggered, so "HELD!"
                            // stays visible until onTapped/onCanceled clears it.
                            if (!pressed && !holdTH.holdTriggered)
                                holdTH.holdProgress = 0.0
                        }
                        // Clear the triggered state on actual release (tapped fires
                        // after the physical lift, even following a long press).
                        onTapped:   { holdTH.holdProgress = 0.0; holdTH.holdTriggered = false }
                        onCanceled: { holdTH.holdProgress = 0.0; holdTH.holdTriggered = false }
                    }

                    Timer {
                        running: holdTHHandler.pressed && !holdTH.holdTriggered
                        interval: 16; repeat: true
                        onTriggered: {
                            var next = holdTH.holdProgress + 0.016 / holdTHHandler.longPressThreshold
                            holdTH.holdProgress = next >= 1.0 ? 1.0 : next
                        }
                    }
                }

                // ── 6. Long Press — MultiPointTouchArea ───────────────────────
                Rectangle {
                    id:     holdMPTA
                    width:  demoStrip.width - demoStrip.boxW * 5
                    height: parent.height
                    color:  "#060e0e"
                    border { color: "#102828"; width: 1 }

                    property real holdProgress:  0.0
                    property bool holdTriggered: false

                    onHoldProgressChanged:  holdMPTAArc.requestPaint()
                    onHoldTriggeredChanged: holdMPTAArc.requestPaint()

                    Column {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 6 }
                        spacing: 1
                        Text { text: "HOLD"; color: "#3a5a7a"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 2; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: "MultiPoint"; color: "#409a8a"; font.pixelSize: 9; anchors.horizontalCenter: parent.horizontalCenter }
                    }

                    Canvas {
                        id: holdMPTAArc; anchors.centerIn: parent; width: 56; height: 56
                        onPaint: {
                            var ctx = getContext("2d"), cx = width/2, cy = height/2, r = 22
                            ctx.clearRect(0, 0, width, height)
                            ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI*2); ctx.strokeStyle = "#081818"; ctx.lineWidth = 5; ctx.stroke()
                            if (holdMPTA.holdProgress > 0) {
                                var s = -Math.PI/2
                                ctx.beginPath(); ctx.arc(cx, cy, r, s, s + holdMPTA.holdProgress * Math.PI * 2)
                                ctx.strokeStyle = holdMPTA.holdTriggered ? "#28D769" : "#2a9a70"; ctx.lineWidth = 5; ctx.stroke()
                            }
                            ctx.beginPath(); ctx.arc(cx, cy, 8, 0, Math.PI*2)
                            ctx.fillStyle = holdMPTA.holdTriggered ? "#28D769" : holdMPTA.holdProgress > 0 ? "#2a9a70" : "#081818"; ctx.fill()
                        }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 6 }
                        text: holdMPTA.holdTriggered ? "HELD!" : " "; color: "#28D769"; font.pixelSize: 12; font.bold: true
                    }

                    MultiPointTouchArea {
                        anchors.fill: parent; minimumTouchPoints: 1; maximumTouchPoints: 1
                        onPressed:  { holdMPTA.holdProgress = 0; holdMPTA.holdTriggered = false; holdMPTATimer.restart() }
                        onReleased: { holdMPTATimer.stop(); holdMPTA.holdProgress = 0; holdMPTA.holdTriggered = false }
                        onCanceled: { holdMPTATimer.stop(); holdMPTA.holdProgress = 0; holdMPTA.holdTriggered = false }
                    }

                    Timer {
                        id: holdMPTATimer; interval: 16; repeat: true
                        onTriggered: {
                            var next = holdMPTA.holdProgress + 0.016 / 0.6
                            if (next >= 1.0) { holdMPTA.holdProgress = 1.0; holdMPTA.holdTriggered = true; stop() }
                            else holdMPTA.holdProgress = next
                        }
                    }
                }
            }

            // ── Trail canvas (fills above the demo strip) ─────────────────────
            Item {
                id:     canvasArea
                anchors { left: parent.left; right: parent.right; top: parent.top; bottom: demoStrip.top }

                property var _live: ({})

                // Instruction bar
                Rectangle {
                    anchors { top: parent.top; left: parent.left; right: parent.right }
                    height: 28
                    color:  "transparent"
                    Text {
                        anchors.centerIn: parent
                        text:           "Trace the dashed line to test the filter"
                        color:          "#445566"
                        font.pixelSize: 13
                    }
                }

                Canvas {
                    id:           canvas
                    anchors.fill: parent

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        // ── Target sine-wave guide ─────────────────────────────
                        var cy     = height * 0.45
                        var amp    = height * 0.09
                        var cycles = 4.0
                        ctx.save()
                        ctx.strokeStyle = "rgba(255,255,255,0.20)"
                        ctx.lineWidth   = 4
                        ctx.setLineDash([16, 10])
                        ctx.beginPath()
                        for (var xi = 0; xi <= width; xi += 4) {
                            var yi = cy + amp * Math.sin(xi / width * cycles * 2 * Math.PI)
                            if (xi === 0) ctx.moveTo(xi, yi); else ctx.lineTo(xi, yi)
                        }
                        ctx.stroke()
                        ctx.setLineDash([])
                        ctx.restore()

                        // ── Touch tracks ───────────────────────────────────────
                        for (var key in touchTracks) {
                            var t = touchTracks[key]
                            if (!t) continue

                            var alpha = t.active ? 1.0 : 0.45
                            var rawColor, dotColor

                            switch (t.classification) {
                            case 'accepted':
                                rawColor = "rgba(40,215,105,"  + alpha + ")"
                                dotColor = "#28D769"
                                break
                            case 'filtered':
                                rawColor = "rgba(255,80,60,"   + alpha + ")"
                                dotColor = "#FF503C"
                                break
                            default:
                                rawColor = "rgba(255,190,0,"   + alpha + ")"
                                dotColor = "#FFBE00"
                            }

                            // Raw trail
                            if (t.rawTrail.length > 1) {
                                ctx.save()
                                ctx.strokeStyle = rawColor
                                ctx.lineWidth   = t.active ? 3 : 2
                                ctx.lineCap     = "round"
                                ctx.lineJoin    = "round"
                                ctx.beginPath()
                                ctx.moveTo(t.rawTrail[0].x, t.rawTrail[0].y)
                                for (var i = 1; i < t.rawTrail.length; i++)
                                    ctx.lineTo(t.rawTrail[i].x, t.rawTrail[i].y)
                                ctx.stroke()
                                ctx.restore()
                            }

                            // Smoothed trail
                            if (root.showSmoothed &&
                                t.classification === 'accepted' &&
                                t.smoothTrail.length > 1)
                            {
                                ctx.save()
                                ctx.strokeStyle = "rgba(80,160,255," + (alpha * 0.85) + ")"
                                ctx.lineWidth   = 2
                                ctx.lineCap     = "round"
                                ctx.lineJoin    = "round"
                                ctx.setLineDash([7, 5])
                                ctx.beginPath()
                                ctx.moveTo(t.smoothTrail[0].x, t.smoothTrail[0].y)
                                for (var j = 1; j < t.smoothTrail.length; j++)
                                    ctx.lineTo(t.smoothTrail[j].x, t.smoothTrail[j].y)
                                ctx.stroke()
                                ctx.setLineDash([])
                                ctx.restore()
                            }

                            // Position dot + label
                            var last = t.rawTrail[t.rawTrail.length - 1]
                            if (!last) continue

                            ctx.save()
                            ctx.globalAlpha = alpha
                            var r = t.state === 'down' ? 18 : t.state === 'up' ? 14 : 11

                            if (t.state === 'up') {
                                ctx.strokeStyle = dotColor
                                ctx.lineWidth   = 3
                                ctx.beginPath()
                                ctx.arc(last.x, last.y, r, 0, Math.PI * 2)
                                ctx.stroke()
                            } else {
                                ctx.fillStyle = dotColor
                                ctx.beginPath()
                                ctx.arc(last.x, last.y, r, 0, Math.PI * 2)
                                ctx.fill()
                                if (t.state === 'down') {
                                    ctx.strokeStyle = "rgba(255,255,255,0.45)"
                                    ctx.lineWidth   = 2
                                    ctx.beginPath()
                                    ctx.arc(last.x, last.y, r + 5, 0, Math.PI * 2)
                                    ctx.stroke()
                                }
                            }

                            ctx.fillStyle = "rgba(255,255,255,0.85)"
                            ctx.font      = "bold 16px sans-serif"
                            ctx.fillText(
                                t.state === 'down' ? '↓ DOWN' :
                                t.state === 'drag' ? '→ DRAG' : '↑ UP',
                                last.x + r + 8, last.y + 5)

                            if (t.filterReason) {
                                ctx.fillStyle = t.classification === 'filtered'
                                                ? "rgba(255,110,90,0.88)"
                                                : "rgba(80,160,255,0.80)"
                                ctx.font      = "14px sans-serif"
                                ctx.fillText(
                                    t.filterReason === 'transient'  ? '(transient)'  :
                                    t.filterReason === 'proximity'  ? '(proximity)'  :
                                    t.filterReason === 'liftResume' ? '(↺ lift)'     : '',
                                    last.x + r + 8, last.y + 21)
                            }
                            ctx.restore()
                        }
                    }
                }

                // ── Legend ─────────────────────────────────────────────────────
                Row {
                    anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8 }
                    spacing: 14
                    Repeater {
                        model: [
                            { color: "#28D769", label: "Accepted (raw)"      },
                            { color: "#50A0FF", label: "Accepted (smoothed)" },
                            { color: "#FFBE00", label: "Pending"             },
                            { color: "#FF503C", label: "Filtered"            }
                        ]
                        Row {
                            spacing: 4
                            anchors.verticalCenter: parent.verticalCenter
                            Rectangle { width: 12; height: 2; radius: 1; color: modelData.color; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: modelData.label; color: "#5a6a80"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
            }
        }
    }
}
