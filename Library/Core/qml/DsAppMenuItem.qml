import QtQuick
import QtQuick.Controls

MenuItem {
    id: control

    contentItem: Item {
        anchors.centerIn: parent

        Text {
            text: control.text
            anchors.left: parent.left
            anchors.leftMargin: 20
        }

        Shortcut {
            id: shortcutHelper
            sequence: control.action.shortcut
        }

        Text {
            text: shortcutHelper.nativeText
            anchors.right: parent.right
            anchors.rightMargin: 20
        }
    }
}
