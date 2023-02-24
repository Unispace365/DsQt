import QtQuick
import QtQuick.VirtualKeyboard
import dsqml
Window {
    id: window
    width: 640
    height: 480
    visible: true
    title: qsTr("Hello World")

    DSSettingsProxy {
        id:colorProxy
        target:"app_settings"
        prefix:"colors"
    }

    Test {
        id:test1
        anchors.fill: parent
        x_model: 0.25
        color: colorProxy.getColor("box");
        Component.onCompleted: {
            var vroot = contentRoot.getChildByName("cms_root");
            for(let i in vroot.children ){
                console.log(vroot.children[i].name);
            }

            console.log(vroot.children.length);
            test1.model = contentRoot.getModel();
        }
    }

    InputPanel {
        id: inputPanel
        z: 99
        x: 0
        y: window.height
        width: window.width

        states: State {
            name: "visible"
            when: inputPanel.active
            PropertyChanges {
                target: inputPanel
                y: window.height - inputPanel.height
            }
        }
        transitions: Transition {
            from: ""
            to: "visible"
            reversible: true
            ParallelAnimation {
                NumberAnimation {
                    properties: "y"
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }
}
