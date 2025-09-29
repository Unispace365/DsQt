import QtQuick
import QtQuick.Layouts
import Dsqt

Item {
    id: content
    anchors.fill: parent

    DsFpsMonitor {
        id: framerate
        interval: 1500
        // force: true
    }

    // Create a clock (DS custom type).
    DsClock {
        id: clock
        x: (parent.width - 250) / 2
        y: (parent.height - 250) / 2
        width: 250
        height: 250
        // speed: 3600
        hourMarkColor: "#ff000000"
        hourHandColor: "#ff000000"
        minuteHandColor: "#ff000000"
        secondHandColor: "transparent"

        // Add a clock label, showing the current scheduled event name.
        Text {
            anchors.top: parent.bottom
            anchors.topMargin: 20
            anchors.horizontalCenter: parent.horizontalCenter
            text: events.current.title
            color: "black"
            font.pixelSize: 14
        }
    }

    // Scheduled event provider (DS custom type), connected to the clock.
    DsEventSchedule {
        id: events
        type: "Scheduled Content" // Only keep events of this type.
        clock: clock
    }

    // Add an fps monitor, showing the frame rate in the top right corner.
    Text {
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.right: parent.right
        anchors.rightMargin: 20
        text: "" + framerate.fps.toFixed(0) + " fps"
        color: "black"
        font.pixelSize: 14
    }
}
