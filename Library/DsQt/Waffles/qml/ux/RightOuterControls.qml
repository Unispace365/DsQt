import QtQuick
import Dsqt.Waffles
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage

DsControlSet {
    id: rightButtons
    edge: DsControlSet.Edge.RightOuter
    width: DsTheme.dp(37)

    ColumnLayout {
        anchors.fill: rightButtons
        spacing: -1
        Item {
            Layout.fillHeight: true
        }

        FullscreenButton {
            Layout.preferredWidth: DsTheme.dp(37)
            Layout.preferredHeight: DsTheme.dp(37)
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            signalObject: rightButtons.signalObject
            edge: rightButtons.edge
            glass: rightButtons.glass
        }

        CloseButton {
            Layout.preferredWidth: DsTheme.dp(37)
            Layout.preferredHeight: DsTheme.dp(37)
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            signalObject: rightButtons.signalObject
            edge: rightButtons.edge
            glass: rightButtons.glass
        }
    }
}

