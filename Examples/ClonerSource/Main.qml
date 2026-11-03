import QtQuick
import Dsqt.Core

DsAppBase {
    id: root
    DsFitView {
        anchors.fill: parent
        preferredWidth: 1920
        preferredHeight: 1080
        fitEnabled: true
        Loader {
            anchors.fill: parent
            source: dsqtWafflesEnabled ? "qml/WafflesView.qml" : "qml/MainView.qml"
        }
    }
}
