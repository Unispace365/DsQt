import QtQuick

Item {
    id: root

    objectName: "textureInputPattern"
    width: 320
    height: 240
    property int phase: 0
    readonly property var quadrantColors: ["#ff0000", "#00ff00", "#0000ff", "#ffff00"]
    readonly property var markerColors: ["#ffffff", "#000000", "#ff00ff", "#00ffff"]
    layer.enabled: true
    layer.smooth: false

    Rectangle {
        width: root.width / 2
        height: root.height / 2
        color: root.quadrantColors[(root.phase + 0) % 4]
    }

    Rectangle {
        x: root.width / 2
        width: root.width / 2
        height: root.height / 2
        color: root.quadrantColors[(root.phase + 1) % 4]
    }

    Rectangle {
        y: root.height / 2
        width: root.width / 2
        height: root.height / 2
        color: root.quadrantColors[(root.phase + 2) % 4]
    }

    Rectangle {
        x: root.width / 2
        y: root.height / 2
        width: root.width / 2
        height: root.height / 2
        color: root.quadrantColors[(root.phase + 3) % 4]
    }

    Rectangle {
        x: 8
        y: 8
        width: 24
        height: 24
        color: root.markerColors[(root.phase + 0) % 4]
    }

    Rectangle {
        x: root.width - width - 8
        y: 8
        width: 24
        height: 24
        color: root.markerColors[(root.phase + 1) % 4]
    }

    Rectangle {
        x: 8
        y: root.height - height - 8
        width: 24
        height: 24
        color: root.markerColors[(root.phase + 2) % 4]
    }

    Rectangle {
        x: root.width - width - 8
        y: root.height - height - 8
        width: 24
        height: 24
        color: root.markerColors[(root.phase + 3) % 4]
    }
}
