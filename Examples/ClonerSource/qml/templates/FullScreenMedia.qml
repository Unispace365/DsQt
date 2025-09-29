import QtQuick
import QtQuick.Controls
import QtMultimedia
import Dsqt
import WhiteLabelWaffles

ContentItem {
    id: root

    // Theme proxy.
    Rectangle {
        id: themeProxy
        anchors.fill: parent
        visible: false

        ShaderEffect {
            anchors.fill: parent
            property variant source: theme // Use the default shader.
        }
    }

    // Background media.
    DsMediaViewer {
        id: media
        anchors.fill: parent
        media: root.model.media
        fillMode: root.model.media_fitting === "Fit" ? Image.PreserveAspectFit : Image.PreserveAspectCrop
        loops: MediaPlayer.Infinite
        cropOverlay: false

        onMediaLoaded: { themeProxy.visible = true }
    }
}
