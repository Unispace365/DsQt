import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import ClonerSource
//an item that uses qt layouts to position items. it has a large font title and below that a horizontal flickable that
//contains a series of buttons with images and text below the image. the buttons should be square and clicking on one
//shows the image fullscreen.
Item {
    anchors.fill: parent
    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        //padding: 10

        Text {
            id: title
            text: "Image Gallery"
            font: Fnt.menuTitle
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }

        Flickable {
            id: flickable
            contentWidth: row.width
            contentHeight: row.height
            clip: true
            Layout.fillWidth: true
            Layout.preferredHeight: 150

            Row {
                id: row
                spacing: 10
                height: parent.height

                Repeater {
                    model: 20 // Number of images

                    Button {
                        width:  20 // Make buttons square with some padding
                        height: 20
                        property int index: index

                        Column {
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 5

                            Image {
                                source: "https://picsum.photos/seed/" + index + "/100" // Placeholder images
                                fillMode: Image.PreserveAspectFit
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: parent.width * 0.8
                                height: parent.width * 0.8
                            }

                            Text {
                                text: "Image " + (index + 1)
                                font.pixelSize: 14
                                horizontalAlignment: Text.AlignHCenter
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }

                        onClicked: {
                            // Show image fullscreen (simple implementation)
                            fullscreenImage.source = "https://picsum.photos/seed/" + index + "/800"
                            fullscreenOverlay.visible = true
                        }
                    }
                }
            }
        }
    }
}
