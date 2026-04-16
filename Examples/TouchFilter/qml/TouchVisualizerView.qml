import QtQuick
import QtQuick.Controls
import Dsqt.Core
import Dsqt.Touch

/// Touch diagnostic view.
///
/// Layout:
///   Left  – large-touch control panel (filter settings + stats + clear)
///   Right – zoomable canvas (pinch to zoom/pan, single finger to test)
///
/// Colour key:
///   Green  solid  – accepted raw path
///   Blue   dashed – accepted smoothed path (EMA)
///   Yellow        – pending (waiting for transient timer)
///   Red           – filtered  (transient / proximity / lift-resume)
///
/// Each track is labelled ↓ DOWN / → DRAG / ↑ UP with a reason note when
/// filtered.  Trails persist until Clear is pressed.
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
                height:   32          // must be explicit — custom delegates don't contribute implicitHeight
                from:     cs.from
                to:       cs.to
                stepSize: cs.stepSize
                value:    cs.value     // initial value

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

    // ── Filter — installed as a real QObject event filter on the window ─────────
    TouchFilter {
        id: filter
        transientThresholdMs:  ctrlTransient.value
        smoothingFactor:       ctrlSmooth.value
        liftResumeThresholdMs: ctrlLift.value
        liftResumeDistancePx:  ctrlLiftDist.value
        proximityFilterPx:     ctrlProximity.value
        filterEnabled:         ctrlEnabled.filterOn

        // touchRaw fires for every physical touch before any filtering.
        // Use it to drive the visualisation canvas and keep _live current.
        // Coordinates arrive in window/scene space; map to canvasArea local.
        onTouchRaw: function(id, x, y, state) {
            var local = canvasArea.mapFromItem(null, x, y)
            canvasArea._live[id] = { x: local.x, y: local.y }

            if      (state === 0) root.recordPress  (id, local.x, local.y)
            else if (state === 1) root.recordMove   (id, local.x, local.y)
            else                  root.recordRelease(id, local.x, local.y)
        }

        // touchAccepted drives all functional decisions (smooth trail, pinch).
        onTouchAccepted: function(id, rawX, rawY, smoothX, smoothY, state) {
            var t = touchTracks[id]

            // Smooth trail — map scene coords to canvas content coords.
            if (t) {
                var sl = canvasArea.mapFromItem(null,smoothX, smoothY)
                var sc = root.toContent(sl.x, sl.y)
                if (state === 0)
                    t.smoothTrail = [sc]
                else
                    t.smoothTrail.push(sc)
            }

            if (state === 0) {
                // Confirmed press — admit to accepted set.
                // Use _live (current position) rather than the original press
                // coords, since the finger may have moved during the pending window.
                var cur = canvasArea._live[id]
                canvasArea._acceptedLive[id] = cur || canvasArea.mapFromItem(null,rawX, rawY)

                var keys = Object.keys(canvasArea._acceptedLive)
                if (keys.length >= 2 && !canvasArea._wasPinching)
                    canvasArea._beginPinch(
                        canvasArea._acceptedLive[keys[0]],
                        canvasArea._acceptedLive[keys[1]])

            } else if (state === 1) {
                // Move — keep accepted position current for smooth pinch updates.
                if (canvasArea._acceptedLive[id] !== undefined) {
                    canvasArea._acceptedLive[id] = canvasArea._live[id]
                                                || canvasArea.mapFromItem(null,rawX, rawY)
                }
                if (canvasArea._wasPinching) {
                    var ids = Object.keys(canvasArea._acceptedLive)
                    if (ids.length >= 2)
                        canvasArea._updatePinch(canvasArea._acceptedLive[ids[0]],
                                                canvasArea._acceptedLive[ids[1]])
                }

            } else if (state === 2) {
                delete canvasArea._acceptedLive[id]
                if (Object.keys(canvasArea._acceptedLive).length < 2)
                    canvasArea._wasPinching = false
            }

            canvas.requestPaint()
        }

        onTouchFiltered: function(id, x, y, state, reason) {
            var t = touchTracks[id]
            if (t) t.filterReason = reason
            // When a lift-resume absorbs the old touch it emits touchFiltered for
            // the old ID with state=2 (virtual release).  confirmRelease is never
            // called for that ID, so touchAccepted(state=2) never fires and the
            // entry would linger in _acceptedLive, causing the next single
            // confirmed touch to see keys.length >= 2 and trigger a spurious pinch.
            if (reason === "liftResume" && state === 2)
                delete canvasArea._acceptedLive[id]
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
            // Lift-resume: new ID bypasses touchAccepted(state=0), so admit
            // it to _acceptedLive directly.
            if (!wasFiltered && reason === "liftResume") {
                var cur = canvasArea._live[id]
                canvasArea._acceptedLive[id] = cur || { x: 0, y: 0 }
            }
            canvas.requestPaint()
        }
    }

    // Install the event filter as soon as this item's window is known.
    onWindowChanged: function(w) { if (w) filter.window = w }
    Component.onCompleted: { var w = Window.window; if (w) filter.window = w }

    // ── Touch track state ─────────────────────────────────────────────────────
    property var  touchTracks:  ({})
    property int  pendingCount:  0
    property int  acceptedCount: 0
    property int  filteredCount: 0

    // Canvas view transform (pan/zoom).
    property real canvasScale: 1.0
    property real canvasPanX:  0.0
    property real canvasPanY:  0.0

    // Show/hide the smoothed (EMA) overlay trail.
    property bool showSmoothed: true

    // ── Coordinate helpers ────────────────────────────────────────────────────
    // Convert canvasArea screen coords → canvas content coords.
    function toContent(sx, sy) {
        return { x: (sx - canvasPanX) / canvasScale,
                 y: (sy - canvasPanY) / canvasScale }
    }

    // ── Track helpers ─────────────────────────────────────────────────────────
    function recordPress(id, sx, sy) {
        var p = toContent(sx, sy)
        touchTracks[id] = {
            id:             id,
            rawTrail:       [p],
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
        t.rawTrail.push(toContent(sx, sy))
        t.state = 'drag'
        canvas.requestPaint()
    }

    function recordRelease(id, sx, sy) {
        var t = touchTracks[id]
        if (!t) return
        t.rawTrail.push(toContent(sx, sy))
        t.state  = 'up'
        t.active = false
        canvas.requestPaint()
    }

    function clearAll() {
        touchTracks   = {}
        pendingCount  = 0
        acceptedCount = 0
        filteredCount = 0
        canvasArea._live          = {}
        canvasArea._acceptedLive  = {}
        canvasArea._wasPinching   = false
        filter.reset()
        canvas.requestPaint()
    }

    function resetZoom() {
        canvasScale = 1.0
        canvasPanX  = 0.0
        canvasPanY  = 0.0
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

                // Title
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
                                { label: "PASS",    value: root.acceptedCount, color: "#28D769" },
                                { label: "WAIT",    value: root.pendingCount,  color: "#FFBE00" },
                                { label: "FILTER",  value: root.filteredCount, color: "#FF503C" }
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
                CtrlSlider {
                    id:       ctrlTransient
                    width:    parent.width
                    label:    "Transient"
                    suffix:   " ms"
                    from:     10;  to: 500;  value: 80;   stepSize: 5
                    Component.onCompleted: value = filterProxy.getInt("transient_ms", 80)
                }
                CtrlSlider {
                    id:       ctrlSmooth
                    width:    parent.width
                    label:    "Smoothing α"
                    suffix:   ""
                    from:     0.0; to: 1.0;  value: 0.30; stepSize: 0.01; decimals: 2
                    Component.onCompleted: value = filterProxy.getFloat("smoothing_alpha", 0.30)
                }
                CtrlSlider {
                    id:       ctrlLift
                    width:    parent.width
                    label:    "Lift resume"
                    suffix:   " ms"
                    from:     20;  to: 400;  value: 120;  stepSize: 5
                    Component.onCompleted: value = filterProxy.getInt("lift_resume_ms", 120)
                }
                CtrlSlider {
                    id:       ctrlLiftDist
                    width:    parent.width
                    label:    "Lift dist"
                    suffix:   " px"
                    from:     0;   to: 200;  value: 25;   stepSize: 5
                    Component.onCompleted: value = filterProxy.getFloat("lift_resume_px", 25)
                }
                CtrlSlider {
                    id:       ctrlProximity
                    width:    parent.width
                    label:    "Proximity"
                    suffix:   " px"
                    from:     0;   to: 200;  value: 0;    stepSize: 5
                    Component.onCompleted: value = filterProxy.getInt("proximity_px", 0)
                }

                // Toggle: filter enabled
                Rectangle {
                    id:     ctrlEnabled
                    property bool filterOn: true
                    width:  parent.width
                    height: 32
                    radius: 4
                    color:  filterOn ? "#1a2d1a" : "#2d1a1a"
                    border { color: filterOn ? "#3d883d" : "#883d3d"; width: 2 }
                    Row {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
                        spacing: 7
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: ctrlEnabled.filterOn ? "#28D769" : "#FF503C"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text:           ctrlEnabled.filterOn ? "Filter enabled" : "Filter disabled"
                            color:          ctrlEnabled.filterOn ? "#dde8ff" : "#5a6a7a"
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    TapHandler { onTapped: ctrlEnabled.filterOn = !ctrlEnabled.filterOn }
                }

                // Toggle: show smoothed trail
                Rectangle {
                    width:  parent.width
                    height: 32
                    radius: 4
                    color:  root.showSmoothed ? "#1a2d4a" : "#111c2e"
                    border { color: root.showSmoothed ? "#3d72c8" : "#1e2a3a"; width: 2 }
                    Row {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
                        spacing: 7
                        Rectangle {
                            width: 8; height: 2; radius: 1
                            color:   "#50A0FF"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text:           "Show smoothed trail"
                            color:          root.showSmoothed ? "#dde8ff" : "#5a6a7a"
                            font.pixelSize: 12
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    TapHandler { onTapped: root.showSmoothed = !root.showSmoothed }
                }

                // Spacer
                Item { width: 1; height: 2 }

                // Action buttons
                Rectangle {
                    width: parent.width; height: 36; radius: 4
                    color: clearHover.containsMouse ? "#2a1520" : "#1a0d14"
                    border { color: "#7a2030"; width: 2 }
                    Text {
                        anchors.centerIn: parent
                        text:           "CLEAR"
                        color:          "#ee4455"
                        font.pixelSize: 13
                        font.bold:      true
                        font.letterSpacing: 3
                    }
                    HoverHandler { id: clearHover }
                    TapHandler  { onTapped: root.clearAll() }
                }

                Rectangle {
                    width: parent.width; height: 36; radius: 4
                    color: zoomHover.containsMouse ? "#162010" : "#0e1a0a"
                    border { color: "#2a5a20"; width: 2 }
                    Text {
                        anchors.centerIn: parent
                        text:           "RESET ZOOM"
                        color:          "#55aa44"
                        font.pixelSize: 13
                        font.bold:      true
                        font.letterSpacing: 2
                    }
                    HoverHandler { id: zoomHover }
                    TapHandler  { onTapped: root.resetZoom() }
                }
            }
        }

        // ── Canvas area ───────────────────────────────────────────────────────
        Item {
            id:     canvasArea
            width:  parent.width - controlsPanel.width
            height: parent.height

            // Instruction + legend overlay at top
            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height:  28
                color:   "transparent"

                Text {
                    anchors.centerIn: parent
                    text:           "Trace the dashed line — pinch to zoom"
                    color:          "#445566"
                    font.pixelSize: 11
                }
            }

            // Zoomable container
            Item {
                id: canvasContainer
                anchors.fill: parent

                transform: [
                    Scale     { xScale: root.canvasScale; yScale: root.canvasScale },
                    Translate { x: root.canvasPanX;       y: root.canvasPanY       }
                ]

                Canvas {
                    id:           canvas
                    anchors.fill: parent

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        // ── Target sine-wave ───────────────────────────────────
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

                            var alpha    = t.active ? 1.0 : 0.45
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

                            // Smoothed trail (accepted + enabled)
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

                            // Position dot + labels
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
            }

            // ── Touch state ────────────────────────────────────────────────────
            // _live         – canvasArea-local position of every raw touch
            //                 (updated by filter.onTouchRaw above)
            // _acceptedLive – only filter-confirmed touches
            //                 (updated by filter.onTouchAccepted above)
            property var   _live:           ({})
            property var   _acceptedLive:   ({})
            property real  _pinchStartScale: 1.0
            property real  _pinchStartPanX:  0.0
            property real  _pinchStartPanY:  0.0
            property point _pinchStartMid:   Qt.point(0, 0)
            property real  _pinchStartDist:  1.0
            property bool  _wasPinching:     false

            function _ptDist(a, b) {
                var dx = a.x - b.x, dy = a.y - b.y
                return Math.sqrt(dx*dx + dy*dy)
            }

            function _beginPinch(p0, p1) {
                _wasPinching        = true
                _pinchStartScale    = root.canvasScale
                _pinchStartPanX     = root.canvasPanX
                _pinchStartPanY     = root.canvasPanY
                _pinchStartMid      = Qt.point((p0.x + p1.x) * 0.5, (p0.y + p1.y) * 0.5)
                _pinchStartDist     = Math.max(1.0, _ptDist(p0, p1))
            }

            function _updatePinch(p0, p1) {
                var curDist = _ptDist(p0, p1)
                var curMid  = Qt.point((p0.x + p1.x) * 0.5, (p0.y + p1.y) * 0.5)

                // Scale delta relative to pinch-start
                var newScale = Math.max(0.25, Math.min(10.0,
                                   _pinchStartScale * curDist / _pinchStartDist))

                // Keep the start-midpoint anchored in content space
                var cx = _pinchStartMid.x
                var cy = _pinchStartMid.y
                var dx = (cx - _pinchStartPanX) / _pinchStartScale
                var dy = (cy - _pinchStartPanY) / _pinchStartScale
                root.canvasScale = newScale
                root.canvasPanX  = cx - dx * newScale + (curMid.x - _pinchStartMid.x)
                root.canvasPanY  = cy - dy * newScale + (curMid.y - _pinchStartMid.y)
                canvas.requestPaint()
            }

            // ── Legend ─────────────────────────────────────────────────────────
            Row {
                anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 8 }
                spacing: 14

                Repeater {
                    model: [
                        { color: "#28D769", label: "Accepted (raw)"       },
                        { color: "#50A0FF", label: "Accepted (smoothed)"  },
                        { color: "#FFBE00", label: "Pending"              },
                        { color: "#FF503C", label: "Filtered"             }
                    ]
                    Row {
                        spacing: 4
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            width: 12; height: 2; radius: 1
                            color: modelData.color
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {
                            text:           modelData.label
                            color:          "#5a6a80"
                            font.pixelSize: 8
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }
    }
}
