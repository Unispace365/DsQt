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

    SystemPalette {
        id: systemActive
        colorGroup: SystemPalette.Active
    }

    DsSettingsTreeModel {
        id: settingsModel
        settingsName: root.settingsName
        onModelReset: expandTimer.restart()
    }

    Timer {
        id: expandTimer
        interval: 0
        onTriggered: treeView.expandRecursively(-1)
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

        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: "Search:"
                font.family: "Segoe UI"
                font.pixelSize: 14
                color: system.text
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                font.family: "Segoe UI"
                font.pixelSize: 14
                placeholderText: "Search by key, path, or value..."
                onTextChanged: {
                    searchResults.model = settingsModel.search(text)
                }
            }
        }

        ListView {
            id: searchResults
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? Math.min(contentHeight, 200) : 0
            visible: searchField.text.length > 0 && count > 0
            clip: true

            delegate: ItemDelegate {
                id: resultDelegate
                width: searchResults.width
                required property var modelData
                required property int index

                contentItem: RowLayout {
                    spacing: 8
                    Text {
                        text: resultDelegate.modelData.path
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        font.bold: !resultDelegate.modelData.isLeaf
                        color: resultDelegate.modelData.isLeaf ? system.text : systemActive.highlight
                        Layout.preferredWidth: 300
                        elide: Text.ElideMiddle
                    }
                    Text {
                        text: resultDelegate.modelData.isLeaf ? "= " + resultDelegate.modelData.value : ""
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        color: system.mid
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }
                    Text {
                        text: resultDelegate.modelData.type
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        color: system.mid
                        Layout.preferredWidth: 60
                        horizontalAlignment: Text.AlignRight
                    }
                }

                onClicked: {
                    let idx = settingsModel.indexForPath(resultDelegate.modelData.path)
                    if (!idx.valid) return
                    treeView.expandToIndex(idx)
                    treeView.forceLayout()
                    let row = treeView.rowAtIndex(idx)
                    if (row >= 0) {
                        treeView.positionViewAtRow(row, Qt.AlignVCenter)
                        treeView.selectionModel.setCurrentIndex(idx, ItemSelectionModel.ClearAndSelect)
                    }
                    searchField.text = ""
                }
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
            pointerNavigationEnabled: false

            delegate: TreeViewDelegate {
                id: treeDelegate

                required property string value
                required property string source
                required property string type
                required property bool isLeaf

                implicitWidth: treeView.width
                //leftPadding: treeDelegate.depth * treeDelegate.indentation + 4

                contentItem: RowLayout {
                    spacing: 12

                    Text {
                        text: model.display
                        font.family: "Segoe UI"
                        font.pixelSize: 14
                        font.bold: !treeDelegate.isLeaf
                        color: treeDelegate.current
                            ? system.highlightedText
                            : (treeDelegate.isLeaf ? system.text : systemActive.highlight)
                        Layout.preferredWidth: 220
                        elide: Text.ElideRight
                    }

                    Text {
                        text: treeDelegate.isLeaf ? treeDelegate.value : ""
                        font.family: "Segoe UI"
                        font.pixelSize: 14
                        color: treeDelegate.current ? system.highlightedText : system.text
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                    }

                    Text {
                        text: treeDelegate.isLeaf ? treeDelegate.type : ""
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        color: treeDelegate.current ? system.highlightedText : system.mid
                        Layout.preferredWidth: 60
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        text: treeDelegate.isLeaf ? treeDelegate.source : ""
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        color: treeDelegate.current ? system.highlightedText : system.mid
                        Layout.preferredWidth: 250
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
    }
}
