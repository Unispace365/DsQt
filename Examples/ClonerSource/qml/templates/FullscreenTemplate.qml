import QtQuick 2.15
import Dsqt

BaseTemplate {
    id: root

    anchors.fill: parent
    property bool interactive
    property int controllerOffset: root.height * 0.05;
    property Item mediaController: Item {}
    onMediaControllerChanged: {
        mediaController.anchors.horizontalCenter = Qt.binding(()=>root.horizontalCenter);
        mediaController.anchors.bottom = Qt.binding(()=>root.bottom);
        mediaController.anchors.bottomMargin = Qt.binding(()=>root.height*0.05<20?20:root.height*0.05);
    }
    Item {
        id: controllerHolder
        visible: root.interactive
        anchors.horizontalCenter: root.horizontalCenter
        anchors.bottom: root.bottom
        anchors.bottomMargin: root.controllerOffset;
    }

    Item {
        id: temp
        anchors.fill: parent
        Rectangle {
            anchors.fill: temp
            color: "#38cfdc"
        }

        Text {
            minimumPixelSize: 120
            text: root.model.name + " : " + root.model.media.filepath
            color: "black"
        }

        Image {
            anchors.fill: parent
            source: root.model.media.filepath
            fillMode: root.model.media_fitting === "Fit" ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        }
    }
}
