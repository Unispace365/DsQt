import QtQuick 2.15
import Dsqt.Waffles

Item {

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
