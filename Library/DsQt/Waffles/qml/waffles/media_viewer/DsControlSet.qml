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

    signal closedViewer(viewer: var)
    signal pinContent()
    signal unpinContent()
    signal startDrawing()
    signal endDrawing()



}
