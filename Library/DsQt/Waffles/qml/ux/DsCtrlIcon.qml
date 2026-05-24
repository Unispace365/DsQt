import QtQuick
import QtQuick.Effects
import QtQuick.VectorImage
import Dsqt.Core
import Dsqt.Waffles

// A flat (no background) icon button: a centred, colourised vector icon with a click area.
// Highlights to the accent colour when `active`; dims and ignores input when disabled.
Item {
    id: ctrlIcon

    // Raw icon path, e.g. "file:///%APP%/data/images/waffles/media/play.svg" (expanded here).
    property string iconName: ""
    property bool active: false
    property real iconSize: 30
    signal clicked()

    implicitWidth: 48
    implicitHeight: 48
    opacity: enabled ? 1.0 : 0.35

    VectorImage {
        anchors.centerIn: parent
        width: ctrlIcon.iconSize
        height: ctrlIcon.iconSize
        source: Ds.env.expandUrl(ctrlIcon.iconName)
        preferredRendererType: VectorImage.CurveRenderer
        layer.enabled: true
        layer.effect: MultiEffect {
            colorization: 1.0
            colorizationColor: ctrlIcon.active ? DsTheme.accent : DsTheme.surfaceText
        }
    }
    MouseArea {
        anchors.fill: parent
        enabled: ctrlIcon.enabled
        cursorShape: Qt.PointingHandCursor
        onClicked: ctrlIcon.clicked()
    }
}
