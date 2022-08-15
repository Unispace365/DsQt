import QtQuick
import QtQuick.VirtualKeyboard
import dsqml
Window {
    id: window
    width: 640
    height: 480
    visible: true
    title: qsTr("Hello World")

    Test {
        id:test1
        anchors.fill: parent
        x_model: 0.25
        Component.onCompleted: {
            ds_content.woot = 20;
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
