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
                    font.pixelSize:  14
                    width:           parent.width - valLabel.implicitWidth - 4
                    elide:           Text.ElideRight
                }
                Text {
                    id:             valLabel
                    text:           cs.decimals > 0
                                    ? sl.value.toFixed(cs.decimals) + cs.suffix
                                    : Math.round(sl.value) + cs.suffix
                    color:          "#dde8ff"
                    font.pixelSize: 14
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
                    font.pixelSize: 11
                    font.bold:      true
                    font.letterSpacing: 3
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
                                    font.pixelSize: 14
                                    font.bold:      true
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text:           modelData.label
                                    color:          Qt.darker(modelData.color, 1.4)
                                    font.pixelSize: 7
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
                        Text { text: ctrlEnabled.filterOn ? "Filter enabled" : "Filter disabled"; color: ctrlEnabled.filterOn ? "#dde8ff" : "#5a6a7a"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
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
                        Text { text: "Show smoothed trail"; color: root.showSmoothed ? "#dde8ff" : "#5a6a7a"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                    }
                    TapHandler { onTapped: root.showSmoothed = !root.showSmoothed }
                }

                Item { width: 1; height: 2 }

                Rectangle {
                    width: parent.width; height: 36; radius: 4
                    color: clearHover.containsMouse ? "#2a1520" : "#1a0d14"
                    border { color: "#7a2030"; width: 2 }
                    Text { anchors.centerIn: parent; text: "CLEAR"; color: "#ee4455"; font.pixelSize: 13; font.bold: true; font.letterSpacing: 3 }
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
            Row {
                id:     demoStrip
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                height: Math.max(150, Math.round(parent.height * 0.30))

                // ── Pinch / Zoom demo ─────────────────────────────────────────
                Rectangle {
                    id:     pinchBox
                    width:  Math.floor(parent.width / 3)
                    height: parent.height
                    color:  "#060d18"
                    clip:   true
                    border { color: "#1a2a3a"; width: 1 }

                    property real contentScale: 1.0
                    property real panX:         0.0
                    property real panY:         0.0

                    Text {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 8 }
                        text: "PINCH / ZOOM"
                        color: "#3a5a7a"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 2
                    }

                    // Zoomable content — 5×5 grid of dots
                    Item {
                        anchors.fill: parent
                        transform: [
                            Translate { x: pinchBox.panX; y: pinchBox.panY },
                            Scale {
                                xScale: pinchBox.contentScale
                                yScale: pinchBox.contentScale
                                origin.x: pinchBox.width  / 2
                                origin.y: pinchBox.height / 2
                            }
                        ]
                        Repeater {
                            model: 25
                            Rectangle {
                                x:      pinchBox.width  / 2 - 60 + (index % 5) * 30
                                y:      pinchBox.height / 2 - 60 + Math.floor(index / 5) * 30
                                width:  6; height: 6; radius: 3
                                color:  "#2a6aaa"
                            }
                        }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8 }
                        text:           pinchBox.contentScale.toFixed(2) + "×"
                        color:          "#2a4a6a"
                        font.pixelSize: 9
                    }

                    PinchHandler {
                        id: pinchHandler
                        target: null
                        grabPermissions: PointerHandler.CanTakeOverFromAnything
                        property bool skipFirst: false
                        onActiveChanged: { if (active) skipFirst = true }
                        onScaleChanged: (delta) => {
                            if (active) pinchBox.contentScale =
                                Math.max(0.15, Math.min(10.0, pinchBox.contentScale * delta))
                        }
                        onActiveTranslationChanged: (t) => {
                            if (!active) return
                            if (skipFirst) { skipFirst = false; return }
                            pinchBox.panX += t.x
                            pinchBox.panY += t.y
                        }
                    }

                    DragHandler {
                        target: null
                        acceptedDevices: PointerDevice.TouchScreen
                        acceptedButtons: Qt.NoButton
                        grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType
                        maximumPointCount: 1
                        property bool skipFirst: false
                        onActiveChanged: { if (active) skipFirst = true }
                        onActiveTranslationChanged: (t) => {
                            if (!active) return
                            if (skipFirst) { skipFirst = false; return }
                            pinchBox.panX += t.x
                            pinchBox.panY += t.y
                        }
                    }
                }

                // ── Double-tap demo ───────────────────────────────────────────
                Rectangle {
                    id:     doubleTapBox
                    width:  Math.floor(parent.width / 3)
                    height: parent.height
                    color:  "#06080e"
                    border { color: "#1a1a28"; width: 1 }

                    property int tapCount: 0

                    Text {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 8 }
                        text: "DOUBLE TAP"
                        color: "#3a5a7a"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 2
                    }

                    // Rings + dot that pulse on double-tap
                    Item {
                        anchors.centerIn: parent
                        width: 64; height: 64

                        Rectangle {
                            id: outerRing
                            anchors.centerIn: parent
                            width: 56; height: 56; radius: 28
                            color: "transparent"
                            border { color: "#1e3a5a"; width: 2 }

                            ScaleAnimator on scale {
                                id: outerAnim
                                from: 1.0; to: 1.6; duration: 250
                                easing.type: Easing.OutCubic
                                running: false
                                onFinished: outerRing.scale = 1.0
                            }
                        }

                        Rectangle {
                            id: innerDot
                            anchors.centerIn: parent
                            width: 20; height: 20; radius: 10
                            color: "#1e4a7a"

                            ScaleAnimator on scale {
                                id: innerAnim
                                from: 1.0; to: 2.0; duration: 200
                                easing.type: Easing.OutBack
                                running: false
                                onFinished: innerDot.scale = 1.0
                            }
                        }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8 }
                        text:           doubleTapBox.tapCount > 0 ? "× " + doubleTapBox.tapCount : " "
                        color:          "#2a5a9a"
                        font.pixelSize: 11
                        font.bold:      true
                    }

                    MultiPointTouchArea {
                        anchors.fill:      parent
                        minimumTouchPoints: 1
                        maximumTouchPoints: 1

                        property real _lastRelease: 0

                        onReleased: (touchPoints) => {
                            var now = Date.now()
                            if (now - _lastRelease < 350) {
                                doubleTapBox.tapCount++
                                outerAnim.restart()
                                innerAnim.restart()
                                _lastRelease = 0   // prevent triple-tap counting again
                            } else {
                                _lastRelease = now
                            }
                        }
                    }
                }

                // ── Long-press demo ───────────────────────────────────────────
                Rectangle {
                    id:     holdBox
                    width:  parent.width - pinchBox.width - doubleTapBox.width
                    height: parent.height
                    color:  "#060e08"
                    border { color: "#1a2a1a"; width: 1 }

                    property real holdProgress:  0.0
                    property bool holdTriggered: false

                    Text {
                        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: 8 }
                        text: "HOLD"
                        color: "#3a5a7a"; font.pixelSize: 9; font.bold: true; font.letterSpacing: 2
                    }

                    // Radial progress arc
                    Canvas {
                        id:           holdArc
                        anchors.centerIn: parent
                        width: 64; height: 64

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var cx = width / 2, cy = height / 2, r = 26

                            // Background ring
                            ctx.beginPath()
                            ctx.arc(cx, cy, r, 0, Math.PI * 2)
                            ctx.strokeStyle = "#112218"
                            ctx.lineWidth   = 6
                            ctx.stroke()

                            // Progress arc
                            if (holdBox.holdProgress > 0) {
                                var start = -Math.PI / 2
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, start,
                                        start + holdBox.holdProgress * Math.PI * 2)
                                ctx.strokeStyle = holdBox.holdTriggered ? "#28D769" : "#2a9a50"
                                ctx.lineWidth   = 6
                                ctx.stroke()
                            }

                            // Centre fill
                            ctx.beginPath()
                            ctx.arc(cx, cy, 10, 0, Math.PI * 2)
                            ctx.fillStyle = holdBox.holdTriggered ? "#28D769"
                                          : holdBox.holdProgress > 0 ? "#2a9a50"
                                          : "#112218"
                            ctx.fill()
                        }
                    }

                    Connections {
                        target: holdBox
                        function onHoldProgressChanged()  { holdArc.requestPaint() }
                        function onHoldTriggeredChanged()  { holdArc.requestPaint() }
                    }

                    Text {
                        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8 }
                        text:           holdBox.holdTriggered ? "HELD!" : " "
                        color:          "#28D769"
                        font.pixelSize: 11
                        font.bold:      true
                    }

                    MultiPointTouchArea {
                        anchors.fill:       parent
                        minimumTouchPoints: 1
                        maximumTouchPoints: 1

                        onPressed:  { holdBox.holdProgress = 0; holdBox.holdTriggered = false; holdFillTimer.restart() }
                        onReleased: { holdFillTimer.stop();     holdBox.holdProgress = 0;       holdBox.holdTriggered = false }
                        onCanceled: { holdFillTimer.stop();     holdBox.holdProgress = 0;       holdBox.holdTriggered = false }
                    }

                    Timer {
                        id:       holdFillTimer
                        interval: 16
                        repeat:   true
                        onTriggered: {
                            var next = holdBox.holdProgress + 0.016 / 0.6
                            if (next >= 1.0) {
                                holdBox.holdProgress  = 1.0
                                holdBox.holdTriggered = true
                                stop()
                            } else {
                                holdBox.holdProgress = next
                            }
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
                        font.pixelSize: 11
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
                            ctx.font      = "bold 14px sans-serif"
                            ctx.fillText(
                                t.state === 'down' ? '↓ DOWN' :
                                t.state === 'drag' ? '→ DRAG' : '↑ UP',
                                last.x + r + 8, last.y + 5)

                            if (t.filterReason) {
                                ctx.fillStyle = t.classification === 'filtered'
                                                ? "rgba(255,110,90,0.88)"
                                                : "rgba(80,160,255,0.80)"
                                ctx.font      = "12px sans-serif"
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
                            Text { text: modelData.label; color: "#5a6a80"; font.pixelSize: 8; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
            }
        }
    }
}
