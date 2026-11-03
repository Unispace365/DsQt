import QtQuick
import QtQuick.Pdf

Item {
    id: root
    property url source
    property int fillMode: Image.PreserveAspectCrop
    property int page: 1
    signal mediaLoaded()
    signal mediaFailed(string detail)

    // Page-navigation API consumed by the media controls. currentPage is 0-based.
    readonly property int pageCount: pdfDoc.pageCount
    readonly property int currentPage: pdfView.currentPage
    function goToPage(n) { pdfView.goToPage(Math.max(0, Math.min(n, pdfDoc.pageCount - 1))) }

    // Restore the current page without changing page selection or the outer viewer geometry.
    function resetView() { pdfView.fitToView(true) }
    onEnabledChanged: if (!enabled) root.resetView()

    // Refit when the viewer is resized (e.g. entering fullscreen).
    onWidthChanged: pdfView.fitToView()
    onHeightChanged: pdfView.fitToView()

    PdfDocument {
        id: pdfDoc
        source: root.source
        onStatusChanged: (status) => {
            if (status === PdfDocument.Error) root.mediaFailed(pdfDoc.error);
            if (status === PdfDocument.Loading) pdfView.loaded = false;
        }
    }

    // Single-page, self-contained view (centres + fits the page); avoids the continuous
    // multi-page TableView layout that rendered the page outside the viewer.
    PdfScrollablePageView {
        id: pdfView
        anchors.fill: parent
        clip: true
        document: pdfDoc
        property bool loaded: false
        readonly property real minimumRenderScale: {
            if (pdfDoc.pageCount <= currentPage || currentPage < 0 || width <= 0 || height <= 0)
                return 0;
            const size = pdfDoc.pagePointSize(currentPage);
            const rotated = Math.abs(pageRotation % 180) > 45 && Math.abs(pageRotation % 180) < 135;
            return size.width > 0 && size.height > 0
                ? Math.min(width / (rotated ? size.height : size.width),
                           height / (rotated ? size.width : size.height)) : 0;
        }
        onRenderScaleChanged: {
            if (minimumRenderScale > 0 && renderScale < minimumRenderScale)
                renderScale = minimumRenderScale;
        }
        // Own PDF pinches so the fit limit also applies during the gesture.
        PinchHandler {
            id: pdfPinch
            parent: pdfGestureSurface
            target: null
            enabled: root.enabled && pdfView.loaded
            grabPermissions: PointerHandler.CanTakeOverFromAnything | PointerHandler.ApprovesCancellation
            property real initialScale: 1
            onActiveChanged: if (active) initialScale = pdfView.renderScale
            onScaleChanged: {
                if (!active) return;
                const scale = Math.max(pdfView.minimumRenderScale, Math.min(10, initialScale * activeScale));
                const ratio = scale / pdfView.renderScale;
                pdfView.contentX = (pdfView.contentX + centroid.position.x) * ratio - centroid.position.x;
                pdfView.contentY = (pdfView.contentY + centroid.position.y) * ratio - centroid.position.y;
                pdfView.renderScale = scale;
                pdfView.returnToBounds();
            }
        }
        leftMargin: Math.max(0, (width - contentWidth) / 2)
        rightMargin: leftMargin
        topMargin: Math.max(0, (height - contentHeight) / 2)
        bottomMargin: topMargin

        function fitToView(fitPage = false) {
            if (root.width <= 0 || root.height <= 0 || pdfDoc.pageCount <= 0)
                return
            cancelFlick()
            resetScale()
            if (fitPage || !root.enabled || root.fillMode === Image.PreserveAspectFit)
                scaleToPage(root.width, root.height)
            else
                scaleToWidth(root.width, root.height)
            contentX = -leftMargin
            contentY = -topMargin
        }

        // status is the rendered-page status; Ready means the document loaded and the page drew.
        onStatusChanged: {
            if (status === Image.Error) root.mediaFailed(qsTr("This page could not be displayed."));
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

    // Observe pinches before the PDF's internal handlers; single-finger input passes through.
    Item {
        id: pdfGestureSurface
        anchors.fill: parent
        z: 1
    }

    readonly property real renderedWidth: width
    readonly property real renderedHeight: height
    readonly property real renderedOffsetX: 0
    readonly property real renderedOffsetY: 0
}
