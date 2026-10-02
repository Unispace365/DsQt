pragma ComponentBehavior: Bound
import QtQuick
import QtMultimedia
import Dsqt.Waffles

Item {
    id: root
    property var captureMedia: ({})
    property bool captureEnabled: true
    property int fillMode: VideoOutput.PreserveAspectFit
    readonly property bool ready: capture.ready
    readonly property bool loading: !ready && error === 0
    readonly property int error: capture.error
    readonly property string errorString: capture.errorString
    readonly property real renderedWidth: output.contentRect.width
    readonly property real renderedHeight: output.contentRect.height
    readonly property real renderedOffsetX: output.contentRect.x
    readonly property real renderedOffsetY: output.contentRect.y
    implicitWidth: capture.frameSize.width > 0 ? capture.frameSize.width : 1920
    implicitHeight: capture.frameSize.height > 0 ? capture.frameSize.height : 1080
    signal mediaLoaded()
    onReadyChanged: if (ready) mediaLoaded()

    DsCaptureSource {
        id: capture
        deviceName: root.captureMedia.deviceName || ""
        deviceId: root.captureMedia.deviceId || ""
        requestedSize: Qt.size(root.captureMedia.width || 1920, root.captureMedia.height || 1080)
        active: root.captureEnabled && root.visible
        videoSink: output.videoSink
    }
    VideoOutput {
        id: output
        anchors.fill: parent
        fillMode: root.fillMode
        mirrored: false
        endOfStreamPolicy: VideoOutput.ClearOutput
    }
}
