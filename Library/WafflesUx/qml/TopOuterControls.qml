import QtQuick
import Dsqt
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage

DsControlSet {
    id: topButtons
    edge: DsControlSet.Edge.TopOuter
    height: titleContainer.height
    property var model: ({})

    RowLayout {
        anchors.fill: topButtons
        spacing: 0



        Item {
            id: titleContainer
            height: 43
            width:topButtons.width
            Rectangle {
                anchors.fill: parent

                topLeftRadius: 20
                topRightRadius: 20
                color: palette.button
            }

            Text {
                id:titleText
                font.family: "Helvetica Neue"
                font.pixelSize: 16
                height:26
                verticalAlignment: Text.AlignVCenter
                x:20
                y: 8.5
                color: palette.text
                text: topButtons.model.title ?? "Media"
            }
        }
        Item { //spacer
            Layout.fillWidth: true
        }
    }
}

