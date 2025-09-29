import QtQuick

BaseTemplate {
    id: root
    anchors.fill: parent
    Rectangle {
        anchors.fill: root
        color: "#f1e4ff"
    }

    Text {
        minimumPixelSize: 120
        text: "Custom Layout Template"
        color: "black"
    }
}
