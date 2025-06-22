pragma ComponentBehavior: Bound
import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts
import Dsqt

Viewer {
    id: root
    viewerType: Viewer.ViewerType.Launcher
    required property var stage
    property alias model: rep.model
    width: childrenRect.width
    height: childrenRect.height


    ColumnLayout {
        Rectangle {
            color: "blue"
            Layout.fillWidth: true
            Layout.preferredHeight: 50
        }
        Repeater {
            id: rep
            Button {
                required property var modelData
                text: modelData?.record_name ?? "woble"
                Layout.fillWidth: true
                onPressed: {
                    let viewerProps = {
                        //"media":modelData
                    }

                    root.stage.openViewer(viewerProps);
                }
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
