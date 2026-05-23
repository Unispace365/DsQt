import QtQuick
import QtWebEngine
import Dsqt.Waffles

// Thin WebEngineView wrapper so DsMediaViewer can drive it like any other media item.
// DsMediaViewer assigns a generic `source` to whatever it loads; WebEngineView's URL
// property is `url`, so we expose `source` here and forward it to `url`.
WebEngineView {
    id: root
    anchors.fill: parent
    clip: true

    // Generic media source set by DsMediaViewer; forwarded to the WebEngineView url.
    property url source
    onSourceChanged: if (String(source).length > 0) root.url = source

    // Dark base so there's no white flash before the page paints (pages still override it).
    backgroundColor: DsTheme.surface

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
