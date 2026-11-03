import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property string destination
    required property string errorMessage
    property int animationIndex: 0
    readonly property list<url> animations: [
        "images/clone1.gif", "images/clone2.gif",
        "images/clone3.gif", "images/clone4.gif"
    ]
    signal cloneAnother()
    signal quitRequested()

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(root.width, 640)
        spacing: 24

        Label {
            Layout.fillWidth: true
            text: root.errorMessage ? qsTr("Couldn't create the project") : qsTr("Project created!")
            font.pixelSize: 28
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
        AnimatedImage {
            objectName: "cloneAnimation"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(280, root.height * 0.4)
            source: root.errorMessage ? "images/error.gif" : root.animations[root.animationIndex]
            sourceSize: Qt.size(560, 280)
            fillMode: Image.PreserveAspectFit
            playing: root.visible
            Accessible.ignored: true
        }
        Label {
            objectName: "cloneResultMessage"
            Layout.fillWidth: true
            text: root.errorMessage || qsTr("Created %1").arg(root.destination)
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAnywhere
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12
            Button {
                objectName: "quitButton"
                text: qsTr("Quit")
                onClicked: root.quitRequested()
            }
            Button {
                objectName: "cloneAnotherButton"
                text: qsTr("Clone another")
                highlighted: true
                onClicked: root.cloneAnother()
            }
        }
    }
}
