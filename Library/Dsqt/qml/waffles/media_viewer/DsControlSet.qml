import QtQuick 2.15
import Dsqt

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
    property real offset
    property real horizontalOffset
    property real verticalOffset

    signal closedViewer(viewer: var)
    signal pinContent()
    signal unpinContent()
    signal startDrawing()
    signal endDrawing()



}
