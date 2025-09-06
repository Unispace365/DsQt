pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtMultimedia
import QtWebEngine
import QtQuick.Pdf
import Dsqt
import QtQuick.VectorImage

// An element that can display images, videos, web content and PDFs.
Item {
    id: root
    anchors.fill: parent

    // Allows setting the source of the media.
    property string source: ""
    // Preferred fill mode.
    property int fillMode: Image.PreserveAspectCrop
    // Preferred loop mode.
    property int loops: 0
    // auto play time based media
    property bool autoPlay: true
    //pdf page number
    property int page: 1
    //type of content to display. if empty, will determine based on file extension
    property string contentType: "" // "image", "image sequence", "web", "pdf", "video", "video stream","youtube?"
    //component array for controllers
    property Component imageController: null
    property Component videoController: null
    property Component webController: null
    property Component pdfController: null
    property Component vectorController: null

    onSourceChanged: {
        contentLoader.sourceComponent = root.getComponentForContent();
    }
    onContentTypeChanged: {
        contentLoader.sourceComponent = root.getComponentForContent();
    }
    signal videoFinished()

    // TODO add additional signals for image load, video start, etc.

    // TODO add media specific controls

    // Function to determine the component based on file extension
    function getComponentForExtension() {

        var ext = source.toLowerCase().split('.').pop();
        if (['mp4', 'avi', 'mov', 'mkv','webm'].indexOf(ext) !== -1) {
            return videoComponent;
        } else if (['jpg', 'jpeg', 'png', 'gif', 'bmp','webp'].indexOf(ext) !== -1) {
            return imageComponent;
        } else if (['svg', 'lottie'].indexOf(ext) !== -1) {
            return vectorComponent;
        } else if (['html', 'htm'].indexOf(ext) !== -1 || source.startsWith("http")) {
            return webViewComponent;
        } else if (['pdf'].indexOf(ext) !== -1) {
            return pdfViewComponent;;
        } else {
            console.log("Unsupported file extension: " + ext);
            return null;
        }
    }

    function getComponentForContent() {
        if(contentType === "video" || contentType === "video stream" ) {
            return videoComponent;
        } else if(contentType === "image" || contentType === "image sequence") {
            return imageComponent;
        } else if(contentType === "vector" || contentType === "vector sequence") {
            return vectorComponent;
        } else if(contentType === "web" || contentType === "youtube") {
            return webViewComponent;
        } else if(contentType === "pdf") {
            return pdfViewComponent;
        } else {
            return getComponentForExtension();
        }
    }

    // Loader to dynamically load the appropriate component
    Loader {
        id: contentLoader
        anchors.fill: parent
        sourceComponent: root.getComponentForContent()
    }

    // Component for Video
    Component {
        id: videoComponent
        Video {
            id: video
            anchors.fill: parent
            source: root.source
            fillMode: root.fillMode
            autoPlay: root.autoPlay
            loops: root.loops

            onStopped: root.videoFinished()
        }
    }

    // Component for Image
    Component {
        id: imageComponent
        AnimatedImage {
            id: image
            anchors.fill: parent
            source: root.source
            fillMode: root.fillMode
            onStatusChanged: {
                if (status === Image.Error) {
                    console.log("Image loading error")
                }
            }

        }
    }

    // Component for VectorImage
    Component {
        id: vectorComponent
        VectorImage {
            id: vectorImage
            preferredRendererType: VectorImage.CurveRenderer
            anchors.fill: parent
            source: root.source
            fillMode: root.fillMode
        }
    }

    // Component for WebEngine
    Component {
        id: webViewComponent
        WebEngineView  {
            id: webView
            anchors.fill: parent
            url: root.source
            onLoadingChanged: (loadingInfo) => {
                if (loadingInfo.status === WebEngineView.LoadFailedStatus) {
                    console.log("WebView loading error: " + loadingInfo.errorString)
                }
            }
        }
    }

    Component {
        id: pdfViewComponent
        Item {
            id: pdfContainer
            anchors.fill: parent
            PdfDocument {
                id: pdfDoc
                source: root.source
            }
            PdfMultiPageView {
                id: pdfView
                anchors.fill: parent
                document: pdfDoc
                currentPage: root.page - 1 // PdfMultiPageView uses 0-based indexing
            }
        }
    }

}
