import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts
import Dsqt

DsViewer {
    id: root
    viewerType: DsViewer.ViewerType.Launcher
    required property var stage
    width: childrenRect.width
    height: childrenRect.height


    ColumnLayout {
        Rectangle {
            color: "blue"
            Layout.fillWidth: true
            Layout.preferredHeight: 50
        }

        Button {
            text: "open waffles window"
            Layout.fillWidth: true
            onPressed: {
                let viewerProps = {

                }

                root.stage.openViewer(viewerProps);
            }
        }
    }
    Behavior on x {
        enabled: !drag.active
        NumberAnimation { duration:200 }
    }
    Behavior on y {
        enabled: !drag.active
        NumberAnimation { duration:200 }
    }

    DragHandler {
        id:drag
    }

}
