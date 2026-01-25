import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
Item {
    ColumnLayout {
        Layout.margins: 10
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 10
            spacing: 20

            Label {
                id: inputIdlabel
                text: "input id:"
                font.pixelSize: 16
                Layout.alignment: Qt.AlignVCenter
            }

            TextField {
                id: inputIdField
                text: ""
                font.pixelSize: 16
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
            }
        }
    }
}
