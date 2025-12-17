import QtQuick
import QtQuick.Shapes
import Dsqt
import ClonerSource

Item {
    id: root

    property real unit: 1 / 3840 * Math.max(parent.width, parent.height)
    property color fillColor: "#cc191f25"
    property color strokeColor: "#2e3439"
    property color textColor: "white"
    property DsClock clock

    Item {
        id: frame
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 30 * unit
        width: 240 * unit
        height: 135 * unit

        Shape {
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer

            ShapePath {
                fillColor: root.fillColor
                strokeColor: root.strokeColor
                strokeWidth: 1
                PathSvg {
                    path: Ds.path.rectangle( 0, 0, frame.width, frame.height, frame.width / 8 )
                }
            }
        }

        Column {
            anchors.centerIn: parent
            width: parent.width
            spacing: 10 //2160 * parent.height

            Text {
                id: time
                text: root.clock.timeHM
                color: root.textColor
                font.family: Fnt.bitcountClk.family
                font.weight: 200
                font.pixelSize: 0.5 * frame.height
                width: parent.width
                height: contentHeight
                horizontalAlignment: Text.AlignHCenter
                onFontChanged: textMeasurer.fit(time)
                onWidthChanged:  textMeasurer.fit(time)
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter

                Text {
                    id: day
                    text: root.clock.dateFull.split(",")[0]
                    color: root.textColor
                    font.family: Fnt.bitcountClk.family
                    font.weight: 500
                    font.pixelSize: 0.25 * time.font.pixelSize
                    width: contentWidth
                    height: contentHeight
                    horizontalAlignment: Text.AlignHCenter
                    //onFontChanged: textMeasurer.fit(date)
                    //onWidthChanged:  textMeasurer.fit(date)
                }
                Text {
                    id: datum
                    text: "," + root.clock.dateFull.split(",")[1] + "," + root.clock.dateFull.split(",")[2]
                    color: root.textColor
                    font.family: Fnt.bitcountClk.family
                    font.weight: 200
                    font.pixelSize: 0.25 * time.font.pixelSize
                    width: contentWidth
                    height: contentHeight
                    horizontalAlignment: Text.AlignHCenter
                    //onFontChanged: textMeasurer.fit(date)
                    //onWidthChanged:  textMeasurer.fit(date)
                }
            }
        }
    }
}
