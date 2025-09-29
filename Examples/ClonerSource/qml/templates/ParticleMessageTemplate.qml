import QtQuick

BaseTemplate {
    id: root
    anchors.fill: parent
    Rectangle {
        anchors.fill: root
        color: "white"
    }

    Text {
        minimumPixelSize: 120
        text: "Particle Message Template"
        color: "black"
    }
}
