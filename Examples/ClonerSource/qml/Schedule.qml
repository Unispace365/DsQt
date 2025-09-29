import QtQuick 6.0
import QtMultimedia 6.0
import QtQuick.Shapes
import QtQuick.Effects
import QtQuick.Controls 6.0
import Dsqt

Item {
    id:sizer

    readonly property real frame: 20      // Frame, in pixels.
    readonly property real padding: 25    // Padding, in pixels.
    readonly property real bar_size: 2    // Bar size, as percentage of vmax.
    readonly property real text_size: 0.3 // Text label size in bars, as a percentage.

    Rectangle {
        id:baseRect
        anchors.fill: parent
        color: "#ffffff"
        enabled: false;

        Text {
            width: contentWidth
            height: contentHeight
            anchors.left: parent.left
            anchors.leftMargin: frame
            text: Ds.env.platformName
            font.pixelSize: 0.7 * frame
            font.bold: true
            color: "gray"
        }

        Text {
            width: contentWidth
            height: contentHeight
            anchors.horizontalCenter: parent.horizontalCenter;
            text: clock.timeHMS + " | " + clock.dateFull
            font.pixelSize: 0.7 * frame
            color: "gray"
        }

        Text {
            x: parent.right - frame
            width: contentWidth
            height: contentHeight
            anchors.right: parent.right
            anchors.rightMargin: frame
            color: "gray"
            text: ""+framerate.fps.toFixed(0)+" fps"
            font.pixelSize: 0.7 * frame
        }
    }

    DsEventSchedule {
        id: allEvents
        clock: clock
    }

    DsEventSchedule {
        id: contentEvents
        type: "Scheduled Content" // Adjust if your CMS uses different event types.
        clock: clock
    }

    DsEventSchedule {
        id: pinboardEvents
        type: "Pinboard Event" // Adjust if your CMS uses different event types.
        clock: clock
    }

    Rectangle {
        id: contentRect
        anchors.fill: baseRect
        anchors.margins: frame
        color: "#80336699"

        DsFpsMonitor {
            id: framerate
            interval: 1500
            // force: true
        }

        // DsClock {
        //     id: clock
        //     // interval: 50
        //     // speed: 1800

        //     // anchors.left: parent.left
        //     // anchors.leftMargin: padding
        //     // anchors.top: parent.top
        //     // anchors.topMargin: padding
        //     // width: schedule.vmax(8)
        //     // height: schedule.vmax(8)
        //     // secondHandColor: "#00000000"
        // }

        // CustomClock {
        //     id: customClock
        //     color: "#ccffffff"
        //     width: schedule.vmax(8)
        //     height: schedule.vmax(8)
        //     anchors.left: parent.left
        //     anchors.leftMargin: padding
        //     anchors.top: parent.top
        //     anchors.topMargin: padding
        //     secondsSinceMidnight: clock.secondsSinceMidnight
        // }

        // Enable for a drop-shadow effect.
        // MultiEffect {
        //     source: schedule
        //     anchors.fill: schedule
        //     shadowEnabled: true
        //     shadowBlur: 1.0
        //     shadowColor: "#80000000"
        //     shadowHorizontalOffset: 5
        //     shadowVerticalOffset: 5
        // }

        Rectangle {
            id: schedule
            anchors.fill: parent
            anchors.margins: padding
            anchors.topMargin: padding
            color: "#00000000"

            /// Returns the width of the current window, or of the current screen, whichever is smallest.
            function viewportWidth() {
                let r = Math.min( Window.width, Screen.width )
                return main.width
            }
            /// Returns the height of the current window, or of the current screen, whichever is smallest.
            function viewportHeight() {
                let r = Math.min( Window.height, Screen.height )
                return main.height
            }
            /// Returns a percentage of either the viewport width or height, whichever is smallest.
            function vmin(percent) {
                let r = 0.01 * percent * Math.min( viewportWidth(), viewportHeight() )
                return r
            }
            /// Returns a percentage of either the viewport width or height, whichever is largest.
            function vmax(percent) {
                let r = 0.01 * percent * Math.max( viewportWidth(), viewportHeight() )
                return r
            }

            Column {
                id: events
                spacing: schedule.vmax(bar_size)

                // Colored bars for each of the events.
                Item {
                    width: schedule.width
                    height: schedule.vmax(bar_size)

                    Repeater {
                        id: content_timeline
                        model: contentEvents.timeline

                        Rectangle {
                            x: (modelData.secondsSinceMidnight / 86400.0) * schedule.width
                            y: 0
                            width: (modelData.durationInSeconds / 86400.0) * schedule.width
                            height: parent.height
                            color: modelData.color(["#ccddcc","#ddeedd"])
                            radius: 0.5 * height
                            border.width: 1
                            border.color: "#30000000"

                            Text {
                                x: parent.radius
                                y: 0
                                width: parent.width - 2 * parent.radius
                                height: contentHeight
                                text: modelData.title
                                font.pixelSize: text_size * parent.height
                                anchors.verticalCenter: parent.verticalCenter;
                                elide: Text.ElideRight // Truncate text with ellipses on the right
                                clip: true // Ensure text doesn't overflow visually
                            }
                        }
                    }
                }

                // Colored bars for each of the events.
                Item {
                    width: schedule.width
                    height: schedule.vmax(bar_size)

                    Repeater {
                        id: pinboard_timeline
                        model: pinboardEvents.timeline

                        Rectangle {
                            x: (modelData.secondsSinceMidnight / 86400.0) * schedule.width
                            y: 0
                            width: (modelData.durationInSeconds / 86400.0) * schedule.width
                            height: parent.height
                            color: modelData.color(["#eeeecc","#ffffdd"])
                            radius: 0.5 * height
                            border.width: 1
                            border.color: "#30000000"

                            Text {
                                x: parent.radius
                                y: 0
                                width: parent.width - 2 * parent.radius
                                height: contentHeight
                                text: modelData.title
                                font.pixelSize: text_size * parent.height
                                anchors.verticalCenter: parent.verticalCenter;
                                elide: Text.ElideRight // Truncate text with ellipses on the right
                                clip: true // Ensure text doesn't overflow visually
                            }
                        }
                    }
                }

                // Colored bars for each of the events.
                Column {
                    spacing: schedule.vmax(0.5)

                    Repeater {
                        id: all_events
                        model: allEvents.events

                        Rectangle {
                            x: ((modelData.secondsSinceMidnight / 86400.0) * schedule.width)
                            y: 0
                            width: (modelData.durationInSeconds / 86400.0) * schedule.width
                            height: schedule.vmax(bar_size)
                            color: modelData.color(["#ccccee","#ddddff"])
                            radius: 0.5 * height
                            border.width: 1
                            border.color: "#30000000"

                            Text {
                                x: parent.radius
                                y: 0
                                width: parent.width - 2 * parent.radius
                                height: contentHeight
                                text: modelData.title
                                font.pixelSize: text_size * parent.height
                                anchors.verticalCenter: parent.verticalCenter;
                                elide: Text.ElideRight // Truncate text with ellipses on the right
                                clip: true // Ensure text doesn't overflow visually
                            }
                        }
                    }
                }
            }

            // Hour grid.
            Repeater {
                id: grid
                model:25

                Shape {
                    x: modelData * (events.width/24.0)
                    y: 0
                    preferredRendererType: Shape.CurveRenderer

                    ShapePath {
                        fillColor: "transparent"
                        strokeColor: "#33000000"
                        strokeWidth: 2

                        PathSvg {
                            path: "M0,0v" + events.height * 1.1
                        }
                    }
                }
            }

            // Hour indicators.
            Repeater {
                id: hour_indicators
                model: 24

                Rectangle {
                    x: modelData * (events.width/24.0)
                    y: events.height * 1.1 - padding;
                    width: 0
                    height: 16
                    color: "transparent"

                    Text {
                        x: 0
                        y: 20;
                        width: contentWidth
                        height: contentHeight
                        text: "" + modelData
                        font.pixelSize: schedule.vmax(0.75)
                        color: "#000000"
                        anchors.horizontalCenter: parent.horizontalCenter;
                    }
                }
            }

            // Current time indicator.
            Shape {
                id: time_indicator
                x: (clock.secondsSinceMidnight / (24 * 3600)) * events.width
                preferredRendererType: Shape.CurveRenderer

                ShapePath {
                    strokeColor: "white"
                    strokeWidth: 2
                    strokeStyle: ShapePath.DashLine
                    dashPattern: [6, 6] // 6px on, 6px off
                    joinStyle: ShapePath.RoundJoin
                    capStyle: ShapePath.RoundCap
                    startX: 0; startY: 0
                    PathLine { x: 0; y: events.height * 1.1 - padding } // Vertical line
                }
            }
        }
    }
}
