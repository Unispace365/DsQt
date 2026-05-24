import QtQuick
import QtQuick.Pdf

Item {
    id: root
    property url source
    property int fillMode: Image.PreserveAspectCrop
    property int page: 1
    signal mediaLoaded()

    // Page-navigation API consumed by the media controls. currentPage is 0-based.
    readonly property int pageCount: pdfDoc.pageCount
    readonly property int currentPage: pdfView.currentPage
    function goToPage(n) { pdfView.goToPage(Math.max(0, Math.min(n, pdfDoc.pageCount - 1))) }

    // Refit when the viewer is resized (e.g. entering fullscreen).
    onWidthChanged: pdfView.fitToView()
    onHeightChanged: pdfView.fitToView()

    PdfDocument { id: pdfDoc; source: root.source }

    // Single-page, self-contained view (centres + fits the page); avoids the continuous
    // multi-page TableView layout that rendered the page outside the viewer.
    PdfScrollablePageView {
        id: pdfView
        anchors.fill: parent
        clip: true
        document: pdfDoc
        property bool loaded: false

        function fitToView() {
            if (root.width <= 0 || root.height <= 0 || pdfDoc.pageCount <= 0)
                return
            if (root.fillMode === Image.PreserveAspectFit)
                scaleToPage(root.width, root.height)
            else
                scaleToWidth(root.width, root.height)
        }

        // status is the rendered-page status; Ready means the document loaded and the page drew.
        onStatusChanged: {
            if (status === Image.Ready && !loaded) {
                loaded = true
                if (root.page > 1)
                    goToPage(root.page - 1)
                fitToView()
                root.mediaLoaded()
            }
        }
        onCurrentPageChanged: fitToView()
    }

    readonly property real renderedWidth: width
    readonly property real renderedHeight: height
    readonly property real renderedOffsetX: 0
    readonly property real renderedOffsetY: 0
}
