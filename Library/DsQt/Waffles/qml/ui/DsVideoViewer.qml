import QtQuick
import QtMultimedia

Video {
    id: root
    property bool loading: true
    signal mediaLoaded()
    signal videoFinished()

    onBufferProgressChanged: {
        if (loading && bufferProgress >= 1) {
            loading = false
            mediaLoaded()
        }
    }
    onStopped: videoFinished()

    readonly property real renderedWidth: {
        if (fillMode === VideoOutput.Stretch) return width
        let ar = implicitWidth / implicitHeight
        if (fillMode === VideoOutput.PreserveAspectFit) return Math.min(width, height * ar)
        if (fillMode === VideoOutput.PreserveAspectCrop) return Math.max(width, height * ar)
        return width
    }
    readonly property real renderedHeight: {
        if (fillMode === VideoOutput.Stretch) return height
        let ar = implicitWidth / implicitHeight
        if (fillMode === VideoOutput.PreserveAspectFit) return Math.min(height, width / ar)
        if (fillMode === VideoOutput.PreserveAspectCrop) return Math.max(height, width / ar)
        return height
    }
    readonly property real renderedOffsetX: (width - renderedWidth) / 2
    readonly property real renderedOffsetY: (height - renderedHeight) / 2
}
