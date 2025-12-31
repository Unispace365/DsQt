import QtQuick
import QtQuick.Controls
import TouchEngineQt 1.0

ApplicationWindow {
    id: root
    width: 1280
    height: 720
    visible: true
    title: "TouchEngine Qt Example - Multiple Instances"
    property var instanceId: DsTouchEngineManager.createInstance();
    property DsTouchEngineInstance engineInstance: DsTouchEngineManager.getInstance(instanceId);

    onInstanceIdChanged: {
        if(instanceId != "" && DsTouchEngineManager.isInitialized) {
            root.engineInstance = DsTouchEngineManager.getInstance(instanceId);
            root.engineInstance.componentPath = "C:/dev/QtTouchEngine/resources/TD/Testbed.tox"
            root.engineInstance.loadComponent();
        }
    }

    Connections {
        target: DsTouchEngineManager
        function onIsInitializedChanged() {
            if(root.instanceId==="" && DsTouchEngineManager.isInitialized) {
                root.instanceId = DsTouchEngineManager.createInstance();
            }
        }
    }



    DsTouchEngineOutputView {
        id: touchView
        enabled: true
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        instanceId: root.instanceId
        outputLink: "op/out1"
        autoUpdate: true
        width: 1920*(parent.height/1080);
        height: parent.height

    }

    DsTouchEngineFloatBufferInput {
        id: floatInput
        instanceId: root.instanceId
        linkName: "op/in2"
        autoUpdate: true
        channelCount: 1
        channelNames: ["R"]
        capacity: 100
        sampleRate: 100
        property real frequency: 1.0
        onInstanceIdChanged: {
            autoUpdate = false;
            floatInput.generateSineWave("R",frequency,1.0,0);
            //floatInput.generateSineWave("G",0.25,1.0,0.21);
            //floatInput.generateSineWave("B",0.25,1.0,0.67);
            autoUpdate = true;
        }
        onFrequencyChanged: {
            floatInput.generateSineWave("R",frequency,1.0,0);
        }
    }

    DsTouchEngineStringInput {
        id: stringInput
        instanceId: root.instanceId
        linkName: "op/in3"
        value: "Hello from QML"
    }

    Rectangle {
        id: outTex
        width: 1024; height: 1024
        x:root.width + 10
        layer.enabled:true
        layer.live: true
        property real yellowPos: 0.33

        gradient: Gradient {
            GradientStop { position: 0.0; color: "red" }
            GradientStop { position: outTex.yellowPos; color: "yellow" }
            GradientStop { position: 1.0; color: "green" }
        }

    }

    //while this "works" it seems to cause a memory leak.
    //DsTouchEngineTextureInput {
       // id: texInput
       // instanceId: root.instanceId
       // linkName: "op/in1"
        //sourceItem: outTex
    //}

    SequentialAnimation {
        running: true
        loops: Animation.Infinite
        NumberAnimation {
            target: floatInput
            property: "frequency"
            from: 0.1
            to: 5.0
            duration: 10000
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: floatInput
            property: "frequency"
            from: 5.0
            to: 0.1
            duration: 10000
            easing.type: Easing.InOutSine
        }
    }

    SequentialAnimation {
        running: true
        loops: Animation.Infinite
        NumberAnimation {
            target: outTex
            property: "yellowPos"
            from: 0.1
            to: 0.99
            duration: 1000
            easing.type: Easing.InOutSine
        }
        NumberAnimation {
            target: outTex
            property: "yellowPos"
            from: 0.99
            to: 0.1
            duration: 1000
            easing.type: Easing.InOutSine
        }
    }

}
