import QtQuick
import Dsqt.Core

DsAppBase {
    id: base

    Loader {
        anchors.fill: parent
        source: "/qt/qml/TouchFilter/qml/TouchVisualizerView.qml"
    }
}
