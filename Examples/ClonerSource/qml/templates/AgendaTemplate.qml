import QtQuick

BaseTemplate {
    id: root

    anchors.fill: parent
    Rectangle {
        anchors.fill: root
        color: "#198cfd"
    }

    Text {
        minimumPixelSize: 120
        text: "AgendaTemplate"
        color: "black"
    }
}
