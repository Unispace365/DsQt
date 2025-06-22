import QtQuick 2.15
import Dsqt

Item {


    required property int edge
    property real offset

    signal closedViewer(viewer: var)
    signal pinContent()
    signal unpinContent()
    signal startDrawing()
    signal endDrawing()

}
