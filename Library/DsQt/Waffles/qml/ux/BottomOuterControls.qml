import QtQuick
import Dsqt
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage

DsControlSet {
    id: bottomButtons
    edge: DsControlSet.Edge.BottomOuter
    height: 37

    //set visible to true to see the container
    visible: false

    RowLayout {
        anchors.fill: BottomButtons
        spacing: -1
        Item { //spacer
            Layout.fillWidth: true
        }

        //add controls here

        Item { //spacer
            Layout.fillWidth: true
        }
    }
}

