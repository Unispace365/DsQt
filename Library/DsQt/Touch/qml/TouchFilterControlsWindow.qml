import QtQuick
import QtQuick.Controls
import QtQuick.Window
import Dsqt.Touch

/// Floating controls window for a TouchFilter instance.
///
/// Opened from DsAppBase's Tools menu ("TouchFilter Debug").
/// Wire it to your filter via the touchFilter property — or set
/// DsAppBase.touchFilterForDebug before the window is first opened.
///
/// The window adjusts all slider positions to reflect the filter's current
/// values when touchFilter is assigned, and writes slider changes back
/// to the filter in real time.
Window {
    id: controls

    title: "TouchFilter Controls"
    minimumWidth: 280
    minimumHeight: 500
    width:  500
    height: 540
    color:  "#0d1520"

    /// The filter to control.  Set DsAppBase.touchFilterForDebug instead of
    /// wiring this directly if you want the menu to manage the window.
    property TouchFilter touchFilter: null

    // ── Internal stats (tracked independently of any overlay) ─────────────────
    property int _pending:  0
    property int _accepted: 0
    property int _filtered: 0

    onTouchFilterChanged: {
        _pending  = 0
        _accepted = 0
        _filtered = 0
        if (touchFilter) {
            slTransient.value  = touchFilter.transientThresholdMs
            slSmoothing.value  = touchFilter.smoothingFactor
            slLiftMs.value     = touchFilter.liftResumeThresholdMs
            slLiftDist.value   = touchFilter.liftResumeDistancePx
            slProximity.value  = touchFilter.proximityFilterPx
            btnEnabled.filterOn = touchFilter.filterEnabled
        }
    }

    Connections {
        target:  controls.touchFilter
        enabled: controls.touchFilter !== null

        function onTouchRaw(id, x, y, state) {
            if (state === 0) controls._pending++
        }

        function onTouchReclassified(id, wasFiltered, reason) {
            var wasPending = controls._pending > 0
            if (wasPending) {
                controls._pending = Math.max(0, controls._pending - 1)
                if (wasFiltered) controls._filtered++
                else             controls._accepted++
            }
        }
    }

    // ── Reusable slider ───────────────────────────────────────────────────────
    component CtrlSlider: Item {
        id: cs
        property string label:    ""
        property string suffix:   ""
        property real   from:     0
        property real   to:       100
        property alias  value:    sl.value
        property real   stepSize: 1
        property int    decimals: 0

        implicitHeight: _col.implicitHeight

        Column {
            id: _col
            anchors { left: parent.left; right: parent.right; top: parent.top }
            spacing: 2

            Row {
                width: parent.width
                Text {
                    text: cs.label; color: "#9aaabf"; font.pixelSize: 15
                    width: parent.width - _val.implicitWidth - 4; elide: Text.ElideRight
                }
                Text {
                    id: _val
                    text:  cs.decimals > 0
                           ? sl.value.toFixed(cs.decimals) + cs.suffix
                           : Math.round(sl.value) + cs.suffix
                    color: "#dde8ff"; font.pixelSize: 15; font.bold: true
                }
            }

            Slider {
                id: sl
                width: parent.width; height: 30
                from: cs.from; to: cs.to; stepSize: cs.stepSize

                background: Rectangle {
                    x: sl.leftPadding; y: sl.topPadding + sl.availableHeight / 2 - height / 2
                    width: sl.availableWidth; height: 5; radius: 3; color: "#1e2a3a"
                    Rectangle { width: sl.visualPosition * parent.width; height: parent.height; radius: parent.radius; color: "#3d72c8" }
                }
                handle: Rectangle {
                    x: sl.leftPadding + sl.visualPosition * (sl.availableWidth - width)
                    y: sl.topPadding  + sl.availableHeight / 2 - height / 2
                    width: 26; height: 26; radius: 13
                    color: sl.pressed ? "#5a8ae0" : "#3d72c8"
                    border { color: "#7aaaee"; width: 2 }
                }
            }
        }
    }

    // ── Content ───────────────────────────────────────────────────────────────
    Column {
        anchors { fill: parent; margins: 12 }
        spacing: 7

        Text {
            text: "TOUCH FILTER"; color: "#4a6a9a"
            font.pixelSize: 12; font.bold: true; font.letterSpacing: 3
        }

        // Reinject badge
        Rectangle {
            width: parent.width; height: 22; radius: 3
            color:  controls.touchFilter && controls.touchFilter.privateReinject ? "#0e2010" : "#1c1408"
            border { color: controls.touchFilter && controls.touchFilter.privateReinject ? "#2a6a30" : "#5a4010"; width: 1 }
            Row {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 7 }
                spacing: 6
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: controls.touchFilter && controls.touchFilter.privateReinject ? "#28D769" : "#c8a828"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text:  controls.touchFilter && controls.touchFilter.privateReinject
                           ? "Private reinject ON" : "Private reinject OFF"
                    color: controls.touchFilter && controls.touchFilter.privateReinject
                           ? "#50aa60" : "#aa8830"
                    font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Stats
        Rectangle {
            width: parent.width; height: 38; radius: 4; color: "#111c2e"
            Row {
                anchors.centerIn: parent
                spacing: 12
                Column {
                    spacing: 1
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: controls._accepted; color: "#28D769"; font.pixelSize: 15; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "PASS";   color: "#157535"; font.pixelSize: 8; font.letterSpacing: 1.5 }
                }
                Column {
                    spacing: 1
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: controls._pending;  color: "#FFBE00"; font.pixelSize: 15; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "WAIT";   color: "#8a6500"; font.pixelSize: 8; font.letterSpacing: 1.5 }
                }
                Column {
                    spacing: 1
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: controls._filtered; color: "#FF503C"; font.pixelSize: 15; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "FILTER"; color: "#8a2018"; font.pixelSize: 8; font.letterSpacing: 1.5 }
                }
            }
        }

        // Sliders
        CtrlSlider {
            id: slTransient; width: parent.width
            label: "Transient"; suffix: " ms"; from: 10; to: 500; stepSize: 5; value: 80
            onValueChanged: if (controls.touchFilter) controls.touchFilter.transientThresholdMs = Math.round(value)
        }
        CtrlSlider {
            id: slSmoothing; width: parent.width
            label: "Smoothing α"; from: 0.0; to: 1.0; stepSize: 0.01; decimals: 2; value: 0.30
            onValueChanged: if (controls.touchFilter) controls.touchFilter.smoothingFactor = value
        }
        CtrlSlider {
            id: slLiftMs; width: parent.width
            label: "Lift resume"; suffix: " ms"; from: 20; to: 400; stepSize: 5; value: 120
            onValueChanged: if (controls.touchFilter) controls.touchFilter.liftResumeThresholdMs = Math.round(value)
        }
        CtrlSlider {
            id: slLiftDist; width: parent.width
            label: "Lift dist"; suffix: " px"; from: 0; to: 200; stepSize: 5; value: 25
            onValueChanged: if (controls.touchFilter) controls.touchFilter.liftResumeDistancePx = value
        }
        CtrlSlider {
            id: slProximity; width: parent.width
            label: "Proximity"; suffix: " px"; from: 0; to: 200; stepSize: 5; value: 0
            onValueChanged: if (controls.touchFilter) controls.touchFilter.proximityFilterPx = value
        }

        // Filter enable toggle
        Rectangle {
            id: btnEnabled
            property bool filterOn: true
            width: parent.width; height: 32; radius: 4
            color:  filterOn ? "#1a2d1a" : "#2d1a1a"
            border { color: filterOn ? "#3d883d" : "#883d3d"; width: 2 }
            Row {
                anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
                spacing: 7
                Rectangle {
                    width: 8; height: 8; radius: 4
                    color: btnEnabled.filterOn ? "#28D769" : "#FF503C"
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text:  btnEnabled.filterOn ? "Filter enabled" : "Filter disabled"
                    color: btnEnabled.filterOn ? "#dde8ff" : "#5a6a7a"
                    font.pixelSize: 14; anchors.verticalCenter: parent.verticalCenter
                }
            }
            TapHandler {
                onTapped: {
                    btnEnabled.filterOn = !btnEnabled.filterOn
                    if (controls.touchFilter) controls.touchFilter.filterEnabled = btnEnabled.filterOn
                }
            }
        }

        // Clear button
        Rectangle {
            width: parent.width; height: 34; radius: 4
            color: _clearHover.containsMouse ? "#2a1520" : "#1a0d14"
            border { color: "#7a2030"; width: 2 }
            Text { anchors.centerIn: parent; text: "CLEAR"; color: "#ee4455"; font.pixelSize: 14; font.bold: true; font.letterSpacing: 3 }
            HoverHandler { id: _clearHover }
            TapHandler {
                onTapped: {
                    controls._pending  = 0
                    controls._accepted = 0
                    controls._filtered = 0
                    if (controls.touchFilter) controls.touchFilter.reset()
                }
            }
        }
    }
}
