import QtQuick
import Dsqt.Waffles

Item {
    id: root

    // Keep background input behind the controls without covering their buttons.
    HoverHandler {
        blocking: true
        property point lastPosition: Qt.point(-1, -1)
        onPointChanged: {
            if (!hovered || (point.scenePosition.x === lastPosition.x && point.scenePosition.y === lastPosition.y))
                return;
            lastPosition = point.scenePosition;
            root.interacted();
        }
    }
    // Observe presses before child buttons/sliders accept them, without taking their grabs.
    Item {
        anchors.fill: parent
        z: 100
        PointHandler {
            id: controlPress
            acceptedButtons: Qt.AllButtons
            onActiveChanged: root.interacted()
            onPointChanged: if (active) root.interacted()
        }
    }
    readonly property bool interactionActive: controlPress.active
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: root.interacted()
        onWheel: event => { root.interacted(); event.accepted = true }
    }
    MultiPointTouchArea {
        anchors.fill: parent
        mouseEnabled: false
        onGestureStarted: gesture => { if (gesture.touchPoints.length > 1) gesture.grab() }
        onPressed: root.interacted()
    }

    enum Edge {
        TopInner,
        TopOuter,
        LeftInner,
        LeftOuter,
        BottomInner,
        BottomOuter,
        RightInner,
        RightOuter,
        Center,
        CenterBack
    }

    required property int edge
    property var config;
    property var model;
    // Glass context supplied by the viewer (source/enabled/tint/blur/refresh); lets control
    // backgrounds match the viewer's glass. Null when the host doesn't provide glass.
    property var glass: null;
    property real offset
    property real horizontalOffset
    property real verticalOffset
    property var signalObject: null

    // Emitted when the user interacts with this control set; the host uses it to keep the
    // controls awake (reset the idle auto-hide timer).
    signal interacted()

    signal closedViewer(viewer: var)
    signal pinContent()
    signal unpinContent()
    signal startDrawing()
    signal endDrawing()

    // Control sets start hidden and fade in/out (driven by the viewer's show/hide + idle logic).
    // visible follows opacity so faded-out controls don't keep receiving input.
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
}
