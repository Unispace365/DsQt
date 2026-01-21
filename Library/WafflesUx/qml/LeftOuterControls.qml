import QtQuick
import Dsqt
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage

DsControlSet {
    id: leftButtons
    edge: DsControlSet.Edge.LeftOuter
    width: 37

    //set visible to true to see the container
    visible: false

    ColumnLayout {
        anchors.fill: rightButtons
        spacing: -1
        Item {
            Layout.fillHeight: true
        }

        //add controls here
    }
}

