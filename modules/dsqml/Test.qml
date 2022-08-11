import QtQuick 2.0


Item {
    id:root
    property var x_model;

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - parent.width* root.x_model;
        height: parent.height - parent.height* root.x_model;
        color: "red"
    }
}
