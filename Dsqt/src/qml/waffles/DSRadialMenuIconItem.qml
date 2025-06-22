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

    Image {
        id:icon
        source: iconItem.modelData.icon
        fillMode: Image.PreserveAspectFit
        anchors.fill: parent
    }

    Text{
        id:text
        horizontalAlignment: Text.AlignHCenter
        anchors.top: icon.bottom
        width:icon.width
        text: iconItem.modelData.text
    }
}
