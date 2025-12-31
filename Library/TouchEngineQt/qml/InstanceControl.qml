import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import TouchEngineQt 

Rectangle {
    id: root
    
    property var instanceId
    property int instanceNumber: 1
    property DsTouchEngineInstance instance: DsTouchEngineManager.getInstance(instanceId)
    
    signal removeRequested()
    onInstanceIdChanged: {
        console.log('instance ID changed to ', instanceId)
        instance = DsTouchEngineManager.getInstance(instanceId)
    }
    color: "#2d2d2d"
    radius: 8
    border.color: instance && instance.isReady ? "#4CAF50" : "#757575"
    border.width: 2
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10
        
        // Header
        RowLayout {
            Layout.fillWidth: true
            
            Label {
                text: "Instance #" + instanceNumber
                font.bold: true
                font.pixelSize: 16
                color: "#ffffff"
            }
            
            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: instance && instance.isReady ? "#4CAF50" : "#757575"
            }
            
            Label {
                text: instance && instance.isReady ? "Ready" : "Not Ready"
                color: "#b0b0b0"
                font.pixelSize: 12
            }
            
            Item { Layout.fillWidth: true }
            
            Button {
                text: "×"
                font.pixelSize: 20
                implicitWidth: 30
                implicitHeight: 30
                onClicked: root.removeRequested()
            }
        }
        
        // TouchEngine View
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#000000"
            border.color: "#404040"
            border.width: 1
            
            DsTouchEngineOutputView {
                id: touchView
                enabled: false
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                instanceId: root.instanceId
                outputLink: "op/out1"
                autoUpdate: true
                width: 1920*(parent.height/1080);
                height: parent.height
                //layer.enabled: true
                // Rectangle {
                //     anchors.centerIn: parent
                //     width: 200
                //     height: 100
                //     visible: !instance || !instance.isLoaded
                //     color: "#1e1e1e"
                //     radius: 4
                    
                //     ColumnLayout {
                //         anchors.centerIn: parent
                //         spacing: 10
                        
                //         Label {
                //             Layout.alignment: Qt.AlignHCenter
                //             text: "No Component Loaded"
                //             color: "#b0b0b0"
                //         }
                        
                //         BusyIndicator {
                //             Layout.alignment: Qt.AlignHCenter
                //             running: instance && !instance.isLoaded && instance.componentPath !== ""
                //             visible: running
                //         }
                //     }
                // }
            }
        }
        
        // Controls
        GroupBox {
            Layout.fillWidth: true
            title: "Component"
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 5
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    TextField {
                        id: pathField
                        Layout.fillWidth: true
                        placeholderText: "Select .tox file..."
                        text: instance ? instance.componentPath : ""
                        readOnly: true
                    }
                    
                    Button {
                        text: "Browse..."
                        onClicked: fileDialog.open()
                    }
                    
                    Button {
                        text: instance && instance.isLoaded ? "Reload" : "Load"
                        enabled: instance && instance.componentPath !== ""
                        onClicked: {
                            if (instance) {
                                if (instance.isLoaded) {
                                    instance.unloadComponent()
                                }
                                instance.loadComponent()
                            }
                        }
                    }
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    visible: instance && instance.errorString !== ""
                    
                    Label {
                        Layout.fillWidth: true
                        text: instance ? instance.errorString : ""
                        color: "#ff5252"
                        wrapMode: Text.WordWrap
                        font.pixelSize: 11
                    }
                }
            }
        }
        
        // Parameters
        GroupBox {
            Layout.fillWidth: true
            title: "Parameters"
            
            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 10
                rowSpacing: 5
                
                Label {
                    text: "Frame Rate:"
                    color: "#b0b0b0"
                }
                
                SpinBox {
                    Layout.fillWidth: true
                    from: 1
                    to: 120
                    value: instance ? instance.frameRate : 60
                    editable: true
                    onValueModified: {
                        if (instance) {
                            instance.frameRate = value
                        }
                    }
                }
                
                Label {
                    text: "Output Link:"
                    color: "#b0b0b0"
                }
                
                TextField {
                    Layout.fillWidth: true
                    text: touchView.outputLink
                    onEditingFinished: touchView.outputLink = text
                }
            }
        }
        
        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            
            Button {
                text: "Request Frame"
                enabled: instance && instance.isReady
                onClicked: touchView.requestFrame()
            }
            
            Button {
                text: "Unload"
                enabled: instance && instance.isLoaded
                onClicked: {
                    if (instance) {
                        instance.unloadComponent()
                    }
                }
            }
            
            Item { Layout.fillWidth: true }
            
            Label {
                text: "Auto Update"
                color: "#b0b0b0"
            }
            
            Switch {
                checked: touchView.autoUpdate
                onToggled: touchView.autoUpdate = checked
            }
        }
    }
    
    FileDialog {
        id: fileDialog
        title: "Select TouchDesigner Component (.tox)"
        nameFilters: ["TouchDesigner Components (*.tox)", "All Files (*)"]
        
        onAccepted: {
            if (instance) {
                // Remove file:/// prefix from URL
                var path = selectedFile.toString()
                path = path.replace(/^(file:\/{3})/, "")
                // Handle Windows drive letters
                if (Qt.platform.os === "windows") {
                    path = path.replace(/^\/([A-Za-z]:)/, "$1")
                }
                instance.componentPath = path
            }
        }
    }
}
