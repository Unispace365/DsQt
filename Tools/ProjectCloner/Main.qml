import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    width: 640
    height: 480
    visible: true
    title: qsTr("Project Cloner")

    ColumnLayout {
        Text {
            id: title
            text: "DsQt (DsCute) ProjectCloner"
        }

        TextField {
            placeholderText: "Project Name"
            Layout.topMargin: 20
        }
    }
}
