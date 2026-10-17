pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import Dsqt.Core
import Dsqt.TouchEngine

Item {
    id: root

    DsTouchEngineSession {
        id: touchEngine
        componentPath: Ds.env.expand("%APP%/data/tox/four_out.tox")
        frameRate: 60
        running: true
    }

    Component.onCompleted: touchEngine.load()

    Rectangle {
        anchors.fill: parent
        color: "#202020"
    }

    GridLayout {
        anchors.fill: parent
        rows: 2
        columns: 2

        Repeater {
            model: ["op/out1", "op/out2", "op/out3", "op/out4"]

            DsTouchEngineView {
                required property string modelData
                session: touchEngine
                outputLink: modelData
                clearColor: "#101010"
                Layout.fillHeight: true
                Layout.fillWidth: true
            }
        }
    }

    Text {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: 12
        }
        visible: touchEngine.errorString.length > 0
        text: touchEngine.errorString
        color: "#ff6b6b"
        wrapMode: Text.Wrap
    }
}
