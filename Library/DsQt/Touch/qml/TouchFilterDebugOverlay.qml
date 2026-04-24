import QtQuick
import Dsqt.Touch

/// Transparent overlay that visualises a TouchFilter's event stream.
///
/// Place it anywhere in the item tree — it fills its parent and is fully
/// transparent to input (enabled: false).  Wire it up via the touchFilter
/// property:
///
///   TouchFilterDebugOverlay {
///       anchors.fill: parent
///       touchFilter: myFilter
///   }
///
/// Colour key:
///   Green  solid  — accepted raw path
///   Blue   dashed — accepted smoothed path (EMA)
///   Yellow        — pending (waiting for transient timer)
///   Red           — filtered (transient / proximity / lift-resume)
///
/// Stats HUD (PASS / WAIT / FILTER) is shown in the top-right corner.
/// Call clearAll() to wipe trails and reset counters.
Item {
    id: root

    /// The filter to observe.  Assign your TouchFilter instance here.
    property TouchFilter touchFilter: null

    /// Show the EMA-smoothed trail alongside the raw trail for accepted touches.
    property bool showSmoothed: true

    /// Wipe all recorded trails, reset counters, and call touchFilter.reset().
    function clearAll() {
        _clearVisuals()
        if (root.touchFilter) root.touchFilter.reset()
    }

    function _clearVisuals() {
        _tracks   = {}
        _pending  = 0
        _accepted = 0
        _filtered = 0
        _canvas.requestPaint()
    }

    enabled: false  // never consumes input events

    // ── Internal state ────────────────────────────────────────────────────────
    property var _tracks:   ({})
    property int _pending:  0
    property int _accepted: 0
    property int _filtered: 0

    onTouchFilterChanged: _clearVisuals()

    // ── Track helpers ─────────────────────────────────────────────────────────
    function _recordPress(id, lx, ly) {
        _tracks[id] = { id: id, rawTrail: [{x: lx, y: ly}], smoothTrail: [],
                        state: 'down', classification: 'pending', filterReason: '', active: true }
        _pending++
        _canvas.requestPaint()
    }

    function _recordMove(id, lx, ly) {
        var t = _tracks[id]; if (!t) return
        t.rawTrail.push({x: lx, y: ly}); t.state = 'drag'
        _canvas.requestPaint()
    }

    function _recordRelease(id, lx, ly) {
        var t = _tracks[id]; if (!t) return
        t.rawTrail.push({x: lx, y: ly}); t.state = 'up'; t.active = false
        _canvas.requestPaint()
    }

    // ── Filter signal connections ─────────────────────────────────────────────
    Connections {
        target:  root.touchFilter
        enabled: root.touchFilter !== null

        function onWasReset() { root._clearVisuals() }

        function onTouchRaw(id, x, y, state) {
            var l = root.mapFromItem(null, x, y)
            if      (state === 0) root._recordPress  (id, l.x, l.y)
            else if (state === 1) root._recordMove   (id, l.x, l.y)
            else                  root._recordRelease(id, l.x, l.y)
        }

        function onTouchAccepted(id, rawX, rawY, smoothX, smoothY, state) {
            var t = root._tracks[id]; if (!t) return
            var sl = root.mapFromItem(null, smoothX, smoothY)
            if (state === 0) t.smoothTrail = [{x: sl.x, y: sl.y}]
            else             t.smoothTrail.push({x: sl.x, y: sl.y})
            _canvas.requestPaint()
        }

        function onTouchFiltered(id, x, y, state, reason) {
            var t = root._tracks[id]; if (t) t.filterReason = reason
            _canvas.requestPaint()
        }

        function onTouchReclassified(id, wasFiltered, reason) {
            var t = root._tracks[id]; if (!t) return
            var wasPending = t.classification === 'pending'
            t.classification = wasFiltered ? 'filtered' : 'accepted'
            t.filterReason   = t.filterReason || reason
            if (wasPending) {
                root._pending = Math.max(0, root._pending - 1)
                if (wasFiltered) root._filtered++
                else             root._accepted++
            }
            _canvas.requestPaint()
        }
    }

    // ── Trail canvas ──────────────────────────────────────────────────────────
    Canvas {
        id: _canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            for (var key in root._tracks) {
                var t = root._tracks[key]
                if (!t) continue

                var alpha = t.active ? 1.0 : 0.45
                var rawColor, dotColor
                switch (t.classification) {
                case 'accepted': rawColor = "rgba(40,215,105," + alpha + ")"; dotColor = "#28D769"; break
                case 'filtered': rawColor = "rgba(255,80,60,"  + alpha + ")"; dotColor = "#FF503C"; break
                default:         rawColor = "rgba(255,190,0,"  + alpha + ")"; dotColor = "#FFBE00"
                }

                // Raw trail
                if (t.rawTrail.length > 1) {
                    ctx.save()
                    ctx.strokeStyle = rawColor
                    ctx.lineWidth   = t.active ? 3 : 2
                    ctx.lineCap = "round"; ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(t.rawTrail[0].x, t.rawTrail[0].y)
                    for (var i = 1; i < t.rawTrail.length; i++)
                        ctx.lineTo(t.rawTrail[i].x, t.rawTrail[i].y)
                    ctx.stroke(); ctx.restore()
                }

                // Smoothed trail
                if (root.showSmoothed && t.classification === 'accepted' && t.smoothTrail.length > 1) {
                    ctx.save()
                    ctx.strokeStyle = "rgba(80,160,255," + (alpha * 0.85) + ")"
                    ctx.lineWidth = 2; ctx.lineCap = "round"; ctx.lineJoin = "round"
                    ctx.setLineDash([7, 5])
                    ctx.beginPath()
                    ctx.moveTo(t.smoothTrail[0].x, t.smoothTrail[0].y)
                    for (var j = 1; j < t.smoothTrail.length; j++)
                        ctx.lineTo(t.smoothTrail[j].x, t.smoothTrail[j].y)
                    ctx.stroke(); ctx.setLineDash([]); ctx.restore()
                }

                // Position dot + state label
                var last = t.rawTrail[t.rawTrail.length - 1]
                if (!last) continue
                ctx.save()
                ctx.globalAlpha = alpha
                var r = t.state === 'down' ? 18 : t.state === 'up' ? 14 : 11

                if (t.state === 'up') {
                    ctx.strokeStyle = dotColor; ctx.lineWidth = 3
                    ctx.beginPath(); ctx.arc(last.x, last.y, r, 0, Math.PI * 2); ctx.stroke()
                } else {
                    ctx.fillStyle = dotColor
                    ctx.beginPath(); ctx.arc(last.x, last.y, r, 0, Math.PI * 2); ctx.fill()
                    if (t.state === 'down') {
                        ctx.strokeStyle = "rgba(255,255,255,0.45)"; ctx.lineWidth = 2
                        ctx.beginPath(); ctx.arc(last.x, last.y, r + 5, 0, Math.PI * 2); ctx.stroke()
                    }
                }

                ctx.fillStyle = "rgba(255,255,255,0.85)"
                ctx.font = "bold 14px sans-serif"
                ctx.fillText(t.state === 'down' ? '↓ DOWN' :
                             t.state === 'drag' ? '→ DRAG' : '↑ UP',
                             last.x + r + 8, last.y + 5)

                if (t.filterReason) {
                    ctx.fillStyle = t.classification === 'filtered'
                                    ? "rgba(255,110,90,0.88)" : "rgba(80,160,255,0.80)"
                    ctx.font = "12px sans-serif"
                    ctx.fillText(t.filterReason === 'transient'  ? '(transient)' :
                                 t.filterReason === 'proximity'  ? '(proximity)' :
                                 t.filterReason === 'liftResume' ? '(↺ lift)'    : '',
                                 last.x + r + 8, last.y + 20)
                }
                ctx.restore()
            }
        }
    }

    // ── Stats HUD — top-right corner ──────────────────────────────────────────
    Rectangle {
        anchors { top: parent.top; right: parent.right; topMargin: 8; rightMargin: 8 }
        radius: 4
        color: "#cc0d1520"
        width:  _statsRow.implicitWidth + 20
        height: 44

        Row {
            id: _statsRow
            anchors.centerIn: parent
            spacing: 12

            Column {
                spacing: 1
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: root._accepted; color: "#28D769"; font.pixelSize: 14; font.bold: true }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PASS";   color: "#157535"; font.pixelSize: 8; font.letterSpacing: 1.5 }
            }
            Column {
                spacing: 1
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: root._pending;  color: "#FFBE00"; font.pixelSize: 14; font.bold: true }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "WAIT";   color: "#8a6500"; font.pixelSize: 8; font.letterSpacing: 1.5 }
            }
            Column {
                spacing: 1
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: root._filtered; color: "#FF503C"; font.pixelSize: 14; font.bold: true }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "FILTER"; color: "#8a2018"; font.pixelSize: 8; font.letterSpacing: 1.5 }
            }
        }
    }

    // ── Legend — bottom center ────────────────────────────────────────────────
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
            delegate: Row {
                required property var modelData
                spacing: 4
                anchors.verticalCenter: parent.verticalCenter
                Rectangle { width: 12; height: 2; radius: 1; color: modelData.color; anchors.verticalCenter: parent.verticalCenter }
                Text { text: modelData.label; color: "#5a6a80"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }
}
