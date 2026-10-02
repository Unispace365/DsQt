pragma ComponentBehavior: Bound
import QtQuick
import Dsqt.Core

Item {
    id: root

    property Component delegate
    property list<var> model
    property var config
    property real startAngle: 0
    property real innerRadius: 250 * 0.5
    property real outerRadius: 460 * 0.5
    property real cInnerRadius: innerRadius
    property real cOuterRadius: outerRadius
    property string spacing: "equal" // fixed, equal
    property string style: "spoke" // spoke, curve, circle
    property real spacingValue: 20
    property color highlightColor: "#00ADF7"
    property int selection: 0
    property point clusterPoint
    property bool fullyOpen: false
    property int closeTimer: appSettings.getInt("quickMenu.closeDelay", 15000)
    property int revealDuration: 250
    property int segmentStagger: 35
    property bool _clusterActive: false
    property int _clusterIndex: -1
    readonly property bool interacting: menuHover.hovered || menuPress.active || root._clusterActive

    property real revealElapsed: 0
    readonly property int revealTotal: Math.max(0, revealDuration)
        + Math.max(0, segments.count - 1) * Math.max(0, segmentStagger)

    signal itemHighlighted(index: int, highlight: bool)
    signal itemSelected(index: int, position: point)

    state: "Off"
    visible: false
    width: cOuterRadius * 2 + 2
    height: cOuterRadius * 2 + 2

    transform: Translate {
        x: -root.width * 0.5
        y: -root.height * 0.5
    }

    DsSettingsProxy {
        id: appSettings
        target: "app_settings"
    }

    Repeater {
        id: segments
        model: root.model
        onCountChanged: {
            if (root.state === "On")
                Qt.callLater(root.startReveal)
        }

        DsQuickMenuSegment {
            id: segment
            required property int index
            required property var modelData

            width: root.width
            height: root.height
            innerRadius: root.cInnerRadius
            outerRadius: root.cOuterRadius
            startAngle: root.startAngle + index * sweepAngle
            sweepAngle: segments.count > 0 ? 360 / segments.count : 0
            entry: modelData
            config: root.config
            iconSource: modelData?.icon ? Ds.env.expandUrl(
                (modelData?.iconPath || root.config?.iconPath || "") + modelData.icon) : ""
            arrowSource: Ds.env.expandUrl("file:///%APP%/data/images/waffles/quick_menu/quickMenuSelectionArrowwt.svg")
            highlightColor: root.highlightColor
            selected: ((root.selection >> index) & 1) !== 0
            interactive: root.state === "On"
            revealProgress: root.revealDuration > 0
                ? Math.max(0, Math.min(1, (root.revealElapsed
                    - index * Math.max(0, root.segmentStagger)) / root.revealDuration))
                : (root.revealElapsed >= index * Math.max(0, root.segmentStagger) ? 1 : 0)

            onHighlighted: highlight => root.setHighlighted(index, highlight || root._clusterIndex === index)
            onSelectedAt: position => root.selectItem(index, position)
        }
    }

    Item {
        id: closeBtn
        objectName: "quickMenuCloseButton"
        width: closeUp.width
        height: closeUp.height
        anchors.centerIn: parent
        enabled: root.state === "On"

        Image {
            id: closeUp
            visible: !closeTapHandler.pressed
            source: Ds.env.expandUrl("file:///%APP%/data/images/waffles/quick_menu/close.svg")
            fillMode: Image.PreserveAspectFit
            sourceSize: Qt.size(24, 24)
        }

        Image {
            visible: closeTapHandler.pressed
            anchors.centerIn: parent
            source: Ds.env.expandUrl("file:///%APP%/data/images/waffles/quick_menu/close_pressed.svg")
            fillMode: Image.PreserveAspectFit
            sourceSize: Qt.size(54, 54)
        }

        TapHandler {
            id: closeTapHandler
            onTapped: root.closeMenu()
        }
    }

    function startReveal() {
        if (root.state !== "On")
            return
        root.fullyOpen = false
        revealAnimation.stop()
        root.revealElapsed = 0
        if (root.revealTotal === 0 || segments.count === 0) {
            root.revealElapsed = root.revealTotal
            root.fullyOpen = true
        } else {
            revealAnimation.start()
        }
    }

    function closeMenu() {
        root.state = "Off"
        root.selection = 0
    }

    function available(index) {
        return index >= 0 && index < root.model.length && root.model[index]?.available !== false
    }

    function selectItem(index, position) {
        if (root.state !== "On" || !root.available(index))
            return
        root.closeMenu()
        root.itemSelected(index, position)
    }

    function setHighlighted(index, highlight) {
        highlight = highlight && root.state === "On" && root.available(index)
        const previous = root.selection
        if (highlight)
            root.selection |= (1 << index)
        else
            root.selection &= ~(1 << index)
        if (previous !== root.selection)
            root.itemHighlighted(index, highlight)
    }

    function refreshHighlights() {
        for (let i = 0; i < segments.count; ++i)
            root.setHighlighted(i, i === root._clusterIndex || segments.itemAt(i)?.pointerHighlighted)
    }

    function rotate(cx, cy, x, y, angle) {
        const radians = angle * Math.PI / 180
        const cosine = Math.cos(radians)
        const sine = Math.sin(radians)
        return [cosine * (x - cx) - sine * (y - cy) + cx,
                sine * (x - cx) + cosine * (y - cy) + cy]
    }

    // Cluster updates are local coordinates; regular mouse clicks use each segment's TapHandler.
    function updatePoint(point) {
        root.clusterPoint = point
        if (root.state !== "On")
            return
        root._clusterActive = true
        const dx = point.x - root.width * 0.5
        const dy = point.y - root.height * 0.5
        const distance = Math.sqrt(dx * dx + dy * dy)
        const angle = ((Math.atan2(dy, dx) * 180 / Math.PI + 90
                        - root.startAngle) % 360 + 360) % 360
        const index = distance > root.innerRadius && distance < root.outerRadius
            && root.model.length > 0 ? Math.floor(angle * root.model.length / 360) : -1
        root._clusterIndex = root.available(index) ? index : -1
        root.refreshHighlights()
    }

    HoverHandler {
        id: menuHover
        enabled: root.state === "On"
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.Stylus
    }

    PointHandler {
        id: menuPress
        enabled: root.state === "On"
        acceptedButtons: Qt.AllButtons
    }

    Timer {
        id: closeDelayTimer
        interval: Math.max(1, root.closeTimer)
        running: root.state === "On" && root.fullyOpen && root.closeTimer > 0 && !root.interacting
        onTriggered: root.closeMenu()
    }

    NumberAnimation {
        id: revealAnimation
        target: root
        property: "revealElapsed"
        from: 0
        to: root.revealTotal
        duration: root.revealTotal
        onFinished: root.fullyOpen = root.state === "On"
    }

    onStateChanged: {
        revealAnimation.stop()
        root.fullyOpen = false
        root._clusterActive = false
        root._clusterIndex = -1
        root.selection = 0
    }

    DsClusterView.onMinimumMetChanged: {
        if (root.DsClusterView.minimumMet) {
            root.state = "On"
            root._clusterActive = true
        }
    }
    DsClusterView.onRemoved: root.closeMenu()
    DsClusterView.onReleased: {
        if (root.state !== "On")
            return
        const index = root._clusterIndex
        const position = root.mapToGlobal(root.clusterPoint)
        root._clusterActive = false
        root._clusterIndex = -1
        root.refreshHighlights()
        root.selectItem(index, position)
    }
    DsClusterView.onUpdated: point => root.updatePoint(point)

    states: [
        State {
            name: "Start"
            PropertyChanges { root.opacity: 0; root.cInnerRadius: 0; root.cOuterRadius: 0 }
        },
        State {
            name: "Off"
            PropertyChanges { root.opacity: 0; root.cInnerRadius: 0; root.cOuterRadius: 0 }
        },
        State {
            name: "On"
            PropertyChanges {
                root.opacity: 1
                root.visible: true
                root.cInnerRadius: root.innerRadius
                root.cOuterRadius: root.outerRadius
            }
        }
    ]
    transitions: [
        Transition {
            from: "Off,Start"
            to: "On"
            SequentialAnimation {
                PropertyAction { properties: "opacity,visible,cInnerRadius,cOuterRadius" }
                ScriptAction { script: root.startReveal() }
            }
        },
        Transition {
            from: "On"
            to: "Off"
            SequentialAnimation {
                ParallelAnimation {
                    NumberAnimation { target: root; property: "opacity"; duration: 200 }
                    NumberAnimation {
                        target: root
                        properties: "cInnerRadius,cOuterRadius"
                        duration: 250
                        easing.type: Easing.OutCubic
                    }
                }
                PropertyAction { target: root; property: "visible"; value: false }
                PropertyAction { target: root; property: "revealElapsed"; value: 0 }
                ScriptAction { script: root.DsClusterView.animateOffFinished() }
            }
        }
    ]
}

