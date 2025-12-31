import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import TouchEngineQt 1.0

ApplicationWindow {
    id: root
    width: 1280
    height: 720
    visible: true
    title: "TouchEngine Qt Example - Multiple Instances"

    color: "#1e1e1e"

    menuBar: MenuBar {
        Menu {
            title: "File"

            Action {
                text: "New Instance"
                onTriggered: addInstance()
            }

            MenuSeparator {}

            Action {
                text: "Exit"
                onTriggered: Qt.quit()
            }
        }

        Menu {
            title: "View"

            Action {
                text: "Grid Layout"
                checkable: true
                checked: gridView.visible
                onTriggered: {
                    gridView.visible = true
                    listView.visible = false
                }
            }

            Action {
                text: "List Layout"
                checkable: true
                checked: listView.visible
                onTriggered: {
                    gridView.visible = false
                    listView.visible = true
                }
            }
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 5

            Label {
                text: "TouchEngine Instances: " + instancesModel.count
                color: "#ffffff"
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Add Instance"
                icon.name: "list-add"
                onClicked: addInstance()
            }

            Button {
                text: "Clear All"
                icon.name: "edit-clear"
                enabled: instancesModel.count > 0
                onClicked: clearAllInstances()
            }
        }
    }

    // Model for instances
    ListModel {
        id: instancesModel
        // Each element has: instanceId (string)
    }

    // Grid view for multiple instances
    ScrollView {
        id: gridView
        anchors.fill: parent
        visible: true

        GridLayout {
            width: gridView.width
            columns: Math.max(1, Math.floor(width / 400))
            columnSpacing: 10
            rowSpacing: 10

            Repeater {
                model: instancesModel

                InstanceControl {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 600
                    Layout.minimumWidth: 380

                    instanceId: model.instanceId
                    instanceNumber: index + 1

                    onRemoveRequested: removeInstance(index)
                }
            }
        }
    }

    // List view for detailed control

    ListView {
        id: listView
        anchors.fill: parent
        visible: false
        spacing: 10

        model: instancesModel

        delegate: Rectangle {}
        /*
            InstanceControl {
            width: listView.width - 20
            height: 500

            instanceId: model.instanceId
            instanceNumber: index + 1

            onRemoveRequested: removeInstance(index)
        }*/
    }

    // Empty state
    Rectangle {
        anchors.centerIn: parent
        width: 300
        height: 200
        visible: instancesModel.count === 0
        color: "#2d2d2d"
        radius: 8

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 20

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "No TouchEngine Instances"
                font.pixelSize: 18
                color: "#ffffff"
            }

            Button {
                Layout.alignment: Qt.AlignHCenter
                text: "Create First Instance"
                onClicked: addInstance()
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "Select TouchDesigner Component (.tox)"
        nameFilters: ["TouchDesigner Components (*.tox)", "All Files (*)"]
        currentFolder: StandardPaths.standardLocations(StandardPaths.DocumentsLocation)[0]
        property var callback: null

        onAccepted: {
            if (callback) {
                callback(selectedFile)
            }
        }
    }

    function addInstance() {
        var instanceId = TouchEngineManager.createInstance()
        instancesModel.append({
            "instanceId": instanceId
        })
    }

    function removeInstance(index) {
        var instanceId = instancesModel.get(index).instanceId
        TouchEngineManager.destroyInstance(instanceId)
        instancesModel.remove(index)
    }

    function clearAllInstances() {
        for (var i = instancesModel.count - 1; i >= 0; i--) {
            removeInstance(i)
        }
    }

    Component.onCompleted: {
        console.log("TouchEngine Qt Application Started")
        console.log("Qt Version:", Qt.version)
        console.log("Available RHI Backends: D3D11, D3D12, Vulkan, OpenGL, Metal")
    }
}
