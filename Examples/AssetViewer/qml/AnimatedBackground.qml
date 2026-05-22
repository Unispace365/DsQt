import QtQuick

Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: "#0d1117"
    }

    Rectangle {
        width: 380; height: 380; radius: width / 2
        color: "#1e0a5c"; opacity: 0.35
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.04; to: root.width * 0.18; duration: 11000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.04; duration: 11000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.08; to: root.height * 0.30; duration: 14000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * 0.08; duration: 14000; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        width: 300; height: 300; radius: width / 2
        color: "#0a1e5c"; opacity: 0.30
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.65; to: root.width * 0.80; duration: 13000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.65; duration: 13000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.55; to: root.height * 0.70; duration: 10000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * 0.55; duration: 10000; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        width: 260; height: 260; radius: width / 2
        color: "#0a4a4a"; opacity: 0.28
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.30; to: root.width * 0.45; duration: 9000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.30; duration: 9000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.75; to: root.height * 0.88; duration: 12000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * 0.75; duration: 12000; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        width: 320; height: 320; radius: width / 2
        color: "#4a0a3c"; opacity: 0.28
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.80; to: root.height * 0.90; duration: 10000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.80; duration: 10000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.05; to: root.height * 0.22; duration: 8000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * 0.05; duration: 8000; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        width: 220; height: 220; radius: width / 2
        color: "#1a3a0a"; opacity: 0.22
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.50; to: root.width * 0.60; duration: 15000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.50; duration: 15000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.35; to: root.height * 0.50; duration: 11000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * 0.35; duration: 11000; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        width: 280; height: 280; radius: width / 2
        color: "#3c1a0a"; opacity: 0.25
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.15; to: root.width * 0.28; duration: 12000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.15; duration: 12000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.60; to: root.height * 0.78; duration: 9500; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * 0.60; duration: 9500; easing.type: Easing.InOutSine }
        }
    }

    Rectangle {
        width: 240; height: 240; radius: width / 2
        color: "#0a1a3c"; opacity: 0.32
        SequentialAnimation on x {
            loops: Animation.Infinite
            NumberAnimation { from: root.width * 0.70; to: root.width * 0.82; duration: 8500; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width * 0.70; duration: 8500; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on y {
            loops: Animation.Infinite
            NumberAnimation { from: root.height * 0.40; to: root.height * 0.55; duration: 13500; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.height * 0.40; duration: 13500; easing.type: Easing.InOutSine }
        }
    }
}
