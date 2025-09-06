pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtMultimedia
import Dsqt

Window {
    width: 1500
    height: 500
    title: "Content Model Viewer"
    visible: true
    color: system.base

    // Allows access to the system color palette.
    SystemPalette {
        id: system
    }

    // Provides access to the content model.
    DsContentModelItemModel {
        id: myModel
    }

    ColumnLayout {
        anchors.fill: parent

        SplitView {
            id: sview
            Layout.fillHeight: true
            Layout.fillWidth: true
            orientation: Qt.Horizontal
            clip: true

            // Tree column.
            TreeView {
                id: data
                model: myModel
                SplitView.maximumHeight: sview.height
                SplitView.minimumWidth: contentWidth
                SplitView.maximumWidth: sview.width / 5 * 2
                alternatingRows: false
                selectionBehavior: TreeView.SingleSelection
                selectionModel: ItemSelectionModel {}

                delegate: TreeViewDelegate {
                    font.family: "Lucida Sans"
                    font.pointSize: 11
                    text: model.display
                    onClicked: {
                        console.log("Clicked:", model.display)
                        let array = []
                        Object.keys(model.contentModel)?.forEach(
                            function (key) {
                                let obj = {
                                    "key": key,
                                    "value": model.contentModel[key]
                                }
                                array.push(obj)
                            })
                        listView.model = array
                        //console.log(JSON.stringify(model.contentModel, null, 2))
                    }
                }
            }

            // Key-Value column.
            Item {
                id: container

                SplitView.maximumHeight: sview.height
                SplitView.minimumWidth: sview.width / 5
                SplitView.maximumWidth: sview.width / 5 * 4
                SplitView.fillWidth: true

                ListView {
                    id: listView
                    anchors.fill: parent
                    // selectionBehavior: TableView.SingleSelection
                    // selectionModel: ItemSelectionModel {}

                    highlightMoveDuration: 0
                    highlightResizeDuration: 0

                    highlight: Rectangle {
                        anchors.fill: parent.currentItem
                        color: system.highlight
                    }

                    delegate: Item {
                        id: itemDelegate
                        height: childrenRect.height
                        width: listView.width

                        required property int index
                        required property var modelData
                        property bool isObj: modelData.value.filepath !== undefined

                        RowLayout {
                            id: rlayout
                            height: childrenRect.height
                            width: listView.width
                            spacing: 5 // Add small spacing for clarity

                            // Key.
                            Text {
                                font.family: "Lucida Sans"
                                font.pointSize: 11
                                lineHeight: 1.2
                                color: system.text
                                text: itemDelegate.modelData.key + ":"
                                elide: Text.ElideRight
                                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                                Layout.preferredWidth: container.width / 4 // Fixed width for the key column
                            }

                            // Value.
                            Text {
                                id: model_value
                                font.family: "Lucida Sans"
                                font.pointSize: 11
                                lineHeight: 1.2
                                color: system.text
                                text: itemDelegate.isObj ? itemDelegate.modelData.value.filepath : itemDelegate.modelData.value
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                                elide: Text.ElideRight
                            }

                            // // Button.
                            // Button {
                            //     id: button
                            //     text: "Preview"
                            //     //palette.base: media.source === itemDelegate.modelData.value.filepath ? system.highlight : system.base
                            //     font.family: "Lucida Sans"
                            //     font.pixelSize: 11
                            //     visible: itemDelegate.isObj
                            //     Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                            //     onClicked: {
                            //         media.source = itemDelegate.isObj ? itemDelegate.modelData.value.filepath : itemDelegate.modelData.value
                            //         info.text = JSON.stringify(itemDelegate.modelData.value, null, 2)
                            //     }
                            // }
                        }

                        // MouseArea to handle clicks
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                listView.currentIndex = index // Set the clicked item as selected

                                //media.source = itemDelegate.isObj ? itemDelegate.modelData.value.filepath : itemDelegate.modelData.value
                                //info.text = JSON.stringify(itemDelegate.modelData.value, null, 2)
                            }
                        }
                    }

                    model: null
                }
            }

            // Media column.
            ColumnLayout {
                SplitView.maximumHeight: sview.height
                SplitView.minimumWidth: sview.width / 5
                SplitView.maximumWidth: sview.width / 5 * 4
                visible: listView.currentItem && listView.currentItem.isObj

                // Media.
                DsMediaViewer {
                    id: media
                    //Layout.alignment: Qt.AlignTop
                    Layout.fillWidth: true
                    Layout.fillHeight: true // Takes remaining space
                    fillMode: Image.PreserveAspectFit
                    loops: MediaPlayer.Infinite
                    // visible: listView.currentItem && listView.currentItem.isObj
                    cropOverlay: true
                    media: listView.currentItem.modelData.value
                }

                // // Info.
                // Text {
                //     Layout.fillWidth: true
                //     height: 300
                //     id: info
                //     font.family: "Lucida Sans"
                //     font.pointSize: 11
                //     lineHeight: 1.2
                //     color: system.text
                //     //wrapMode: Text.Wrap // Ensure text wraps to avoid horizontal scrolling
                // }
            }

            // Dragable handles.
            handle: Rectangle {
                implicitWidth: 16
                implicitHeight: 16
                color: "transparent"

                Rectangle {
                    color: system.button
                    width: 2
                    height: parent.height
                    anchors.left: parent.left
                }

                Rectangle {
                    color: system.button
                    width: 2
                    height: parent.height
                    anchors.centerIn: parent
                }

                Rectangle {
                    color: system.button
                    width: 2
                    height: parent.height
                    anchors.right: parent.right
                }
            }
        }

        Button {
            font.family: "Lucida Sans"
            font.pointSize: 11
            text: "Reload" + (myModel?.isDirty ? " (dirty)" : "")
            Layout.fillWidth: true
            onClicked: {
                myModel.reload()
                console.log("Reloaded model")
            }
        }
    }
}
