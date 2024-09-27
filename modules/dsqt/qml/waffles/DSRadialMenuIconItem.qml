import QtQuick 2.15

Item {
    id: iconItem
    width: 64
    height: 64
    required property var modelData;
    property var text: modelData.text;
    onTextChanged: {
        console.log("text changed to: " + text);
    }
    Rectangle {
        color: "blue"
        anchors.fill: parent
    }

    Text{
        horizontalAlignment: Text.AlignHCenter
        anchors.horizontalCenter: parent.horizontalCenter
        text: modelData.text
    }
}
