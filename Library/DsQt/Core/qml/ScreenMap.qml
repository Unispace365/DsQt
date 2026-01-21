// ScreenMap.qml
import QtQuick 2.15
import QtQuick.Window 2.15  // For Screen attached properties

Item {
    id: screenMap

    // The element's width and height are set by the user (e.g., ScreenMap { width: 300; height: 200 })
    // Position is also set by the user via x and y properties (e.g., x: 10; y: 10)

    // Computed properties for virtual desktop bounds
    property real minX: 0
    property real minY: 0
    property real maxRight: 1
    property real maxBottom: 1
    property real totalVirtualWidth: 1
    property real totalVirtualHeight: 1
    property real scaleFactor: Math.min(width / totalVirtualWidth, height / totalVirtualHeight)
    //property real offsetX: (width - totalVirtualWidth * scaleFactor) / 2
    //property real offsetY: (height - totalVirtualHeight * scaleFactor) / 2

    // Compute bounds on completion (assuming screens don't change dynamically)
    Component.onCompleted: {
        let mx = Number.POSITIVE_INFINITY
        let my = Number.POSITIVE_INFINITY
        let mr = Number.NEGATIVE_INFINITY
        let mb = Number.NEGATIVE_INFINITY

        for (let screen of Qt.application.screens) {
            mx = Math.min(mx, screen.virtualX)
            my = Math.min(my, screen.virtualY)
            mr = Math.max(mr, screen.virtualX + screen.width)
            mb = Math.max(mb, screen.virtualX + screen.height)
        }

        minX = mx
        minY = my
        maxRight = mr
        maxBottom = mb
        totalVirtualWidth = mr - mx
        totalVirtualHeight = mb - my
    }

    // Draw rectangles for each screen
    Repeater {
        model: Qt.application.screens
        delegate: Rectangle {
            x: (modelData.virtualX - minX) * scaleFactor
            y: (modelData.virtualY - minY) * scaleFactor
            width: modelData.width * scaleFactor
            height: modelData.height * scaleFactor

            color: "transparent"
            border.color: "white"
            border.width: (modelData.name === Screen.name ? 3 : 1)  // Thicker border for current screen
        }
    }
}
