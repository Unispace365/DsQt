import QtQuick

BaseTemplate {
    id: root
    anchors.fill: parent
    Rectangle {
        anchors.fill: root
        color: "#cacacc"
    }

    Text {
        minimumPixelSize: 120
        text: "Message Template"
        color: "black"
    }
}
