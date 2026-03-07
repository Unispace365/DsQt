pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dsqt.Core

Window {
    id: root
    width: 1200
    height: 600
    title: "Settings Viewer"
    visible: true
    color: system.base

    property string settingsName: ""

    SystemPalette {
        id: system
    }

    DsSettingsTreeModel {
        id: settingsModel
        settingsName: root.settingsName
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Collection:"
                font.family: "Segoe UI"
                font.pixelSize: 14
                color: system.text
            }

            ComboBox {
                id: collectionPicker
                Layout.preferredWidth: 200
                font.family: "Segoe UI"
                font.pixelSize: 14
                model: settingsModel.availableSettings
                currentIndex: {
                    if (root.settingsName === "") return 0
                    let idx = settingsModel.availableSettings.indexOf(root.settingsName)
                    return idx >= 0 ? idx : 0
                }
                onActivated: {
                    settingsModel.settingsName = currentText
                    fileFilter.currentIndex = 0
                }
                Component.onCompleted: {
                    if (root.settingsName === "" && count > 0) {
                        settingsModel.settingsName = currentText
                    }
                }
            }

            Label {
                text: "Source file:"
                font.family: "Segoe UI"
                font.pixelSize: 14
                color: system.text
            }

            ComboBox {
                id: fileFilter
                Layout.fillWidth: true
                font.family: "Segoe UI"
                font.pixelSize: 14
                model: ["(All - Effective)"].concat(settingsModel.loadedFiles)
                onCurrentIndexChanged: {
                    settingsModel.filterFile = currentIndex === 0
                        ? ""
                        : settingsModel.loadedFiles[currentIndex - 1]
                }
            }

            Button {
                text: "Reload"
                font.family: "Segoe UI"
                font.pixelSize: 14
                onClicked: settingsModel.reload()
            }
        }

        TreeView {
            id: treeView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: settingsModel
            alternatingRows: true
            clip: true
            selectionBehavior: TreeView.SingleSelection
            selectionModel: ItemSelectionModel {}

            delegate: TreeViewDelegate {
                id: treeDelegate

                required property string value
                required property string source
                required property string type
                required property bool isLeaf

                contentItem: RowLayout {
                    spacing: 12

                    Text {
                        text: model.display
                        font.family: "Segoe UI"
                        font.pixelSize: 14
                        font.bold: !treeDelegate.isLeaf
                        color: treeDelegate.isLeaf ? system.text : system.highlight
                        Layout.preferredWidth: 220
                        elide: Text.ElideRight
                    }

                    Text {
                        text: treeDelegate.isLeaf ? treeDelegate.value : ""
                        font.family: "Segoe UI"
                        font.pixelSize: 14
                        color: system.text
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: treeDelegate.isLeaf ? treeDelegate.type : ""
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        color: system.mid
                        Layout.preferredWidth: 60
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        text: treeDelegate.isLeaf ? treeDelegate.source : ""
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        color: system.mid
                        Layout.preferredWidth: 250
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
    }
}
