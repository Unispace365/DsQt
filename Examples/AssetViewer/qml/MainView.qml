import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import Dsqt.Waffles
import AssetViewer

Item {
    anchors.fill: parent

    property int nextSeed: 1

    DsWaffleStage {
        id: stage
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Text {
                id: title
                text: "Image Gallery"
                font: Fnt.menuTitle
                color: "#e0e0e0"
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }

            Row {
                spacing: 12
                Layout.alignment: Qt.AlignHCenter

                Button {
                    text: "Add Image Viewer"
                    onClicked: {
                        let seed = nextSeed++
                        stage.createViewer({
                            "model": { "title": "Image " + seed, "media": { "filepath": "https://picsum.photos/seed/" + seed + "/800/600", "type": "image", "width": 800, "height": 600 } },
                            "viewerWidth": 480,
                            "viewerHeight": 360,
                            "stage": stage
                        })
                    }
                }

                Button {
                    text: "Add Web Viewer"
                    onClicked: {
                        stage.createViewer({
                            "model": { "title": "Web Viewer", "media": { "filepath": "https://example.com", "type": "web" } },
                            "viewerWidth": 640,
                            "viewerHeight": 480,
                            "stage": stage
                        })
                    }
                }
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
                        model: 20

                        Button {
                            width: 120
                            height: 120
                            property int imgIndex: index

                            Column {
                                anchors.fill: parent
                                anchors.margins: 5
                                spacing: 5

                                Image {
                                    source: "https://picsum.photos/seed/" + imgIndex + "/100"
                                    fillMode: Image.PreserveAspectFit
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: parent.width * 0.8
                                    height: parent.width * 0.8
                                }

                                Text {
                                    text: "Image " + (imgIndex + 1)
                                    font.pixelSize: 14
                                    color: "#e0e0e0"
                                    horizontalAlignment: Text.AlignHCenter
                                    anchors.horizontalCenter: parent.horizontalCenter
                                }
                            }

                            onClicked: {
                                let seed = imgIndex + 1
                                stage.createViewer({
                                    "model": { "title": "Image " + seed, "media": { "filepath": "https://picsum.photos/seed/" + seed + "/800/600", "type": "image", "width": 800, "height": 600 } },
                                    "viewerWidth": 480,
                                    "viewerHeight": 360,
                                    "stage": stage
                                })
                            }
                        }
                    }
                }
            }
        }
    }
}
