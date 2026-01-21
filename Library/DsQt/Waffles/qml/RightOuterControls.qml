import QtQuick
import Dsqt
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage

DsControlSet {
    id: rightButtons
    edge: DsControlSet.Edge.RightOuter
    width: 37

    ColumnLayout {
        anchors.fill: rightButtons
        spacing: -1
        Item {
            Layout.fillHeight: true
        }

        FullscreenButton {
            Layout.preferredWidth: 37
            Layout.preferredHeight: 37
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            signalObject: rightButtons.signalObject
            edge: rightButtons.edge
        }

        CloseButton {
            Layout.preferredWidth: 37
            Layout.preferredHeight: 37
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            signalObject: rightButtons.signalObject
            edge: rightButtons.edge

        }
    }
}

