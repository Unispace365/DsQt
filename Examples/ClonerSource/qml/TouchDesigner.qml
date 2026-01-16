import QtQuick
import QtQuick.Layouts
import Dsqt
import TouchEngineQt

Item {

    id: root
    anchors.fill: parent

    //We create two properties for each instance. One is the instanceId in the manager and the other is the
    //actual instance. The instanceId should be empty ("") if the Manager isn't ready to create and instance.
    property var instanceId: DsTouchEngineManager.createInstance()
    property DsTouchEngineInstance engineInstance: DsTouchEngineManager.getInstance(instanceId)

    //Because the manager may not be ready when this object is created,
    //we connect to the managers initialization signal and if we don't have an instanceID
    //we call createInstance() to get a new instance.
    Connections {
        target: DsTouchEngineManager
        function onIsInitializedChanged() {
            if(root.instanceId==="" && DsTouchEngineManager.isInitialized) {
                root.instanceId = DsTouchEngineManager.createInstance();
            }
        }
    }

    //when the instanceId changes update the reference to the engine and in
    //this case we also load a tox.
    //The tox needs to be a file on disk, i.e. not a URL or in a qrc.
    onInstanceIdChanged: {
        if(instanceId != "" && DsTouchEngineManager.isInitialized) {
            //update the instance
            root.engineInstance = DsTouchEngineManager.getInstance(instanceId);
            //load a tox.
            root.engineInstance.componentPath = Ds.env.expand("%APP%/data/tox/four_out.tox")
            root.engineInstance.loadComponent();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#202020"
    }

    GridLayout {
        id: grid
        anchors.fill: parent
        rows: 2
        columns: 2

        //A DsTouchEngineTextureView is the Qt connection to an TOP out operator
        DsTouchEngineTextureView {
            id: touchView
            enabled: true
            Layout.row: 0
            Layout.column: 0
            instanceId: root.instanceId
            outputLink: "op/out1"
            autoUpdate: true
            Layout.fillHeight: true
            Layout.fillWidth: true

        }
        DsTouchEngineTextureView {
            id: touchView2
            enabled: true
            Layout.row: 0
            Layout.column: 1
            instanceId: root.instanceId
            outputLink: "op/out2"
            autoUpdate: true
            Layout.fillHeight: true
            Layout.fillWidth: true

        }
        DsTouchEngineTextureView {
            id: touchView3
            enabled: true
            Layout.row: 1
            Layout.column: 0
            instanceId: root.instanceId
            outputLink: "op/out3"
            autoUpdate: true
            Layout.fillHeight: true
            Layout.fillWidth: true

        }
        DsTouchEngineTextureView {
            id: touchView4
            enabled: true
            Layout.row: 1
            Layout.column: 1
            instanceId: root.instanceId
            outputLink: "op/out4"
            autoUpdate: true
            Layout.fillHeight: true
            Layout.fillWidth: true

        }
    }

}
