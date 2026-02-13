import QtQuick
import QtWebEngine

WebEngineView {
    id: root
    signal mediaLoaded()

    onLoadingChanged: (loadingInfo) => {
        if (loadingInfo.status === WebEngineView.LoadFailedStatus)
            console.log("WebView loading error: " + loadingInfo.errorString)
        else if (loadingInfo.status === WebEngineView.LoadSucceededStatus)
            root.mediaLoaded()
    }

    readonly property real renderedWidth: width
    readonly property real renderedHeight: height
    readonly property real renderedOffsetX: 0
    readonly property real renderedOffsetY: 0
}
