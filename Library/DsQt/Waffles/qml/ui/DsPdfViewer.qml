import QtQuick
import QtQuick.Pdf

Item {
    id: root
    property url source
    property int fillMode: Image.PreserveAspectCrop
    property int page: 1
    signal mediaLoaded()

    PdfDocument { id: pdfDoc; source: root.source }
    PdfMultiPageView {
        id: pdfView
        anchors.fill: parent
        document: pdfDoc
        property bool loading: true
        Component.onCompleted: {
            if (pdfDoc.pageCount > 0) {
                pdfView.goToPage(root.page - 1)
                fitToView()
                if (loading) { loading = false; root.mediaLoaded() }
            }
        }
        onCurrentPageChanged: fitToView()
        function fitToView() {
            if (root.fillMode === Image.PreserveAspectFit)
                pdfView.scaleToPage(root.width, root.height)
            else if (root.fillMode === Image.PreserveAspectCrop)
                pdfView.scaleToWidth(root.width, root.height)
        }
    }

    readonly property real renderedWidth: width
    readonly property real renderedHeight: height
    readonly property real renderedOffsetX: 0
    readonly property real renderedOffsetY: 0
}
