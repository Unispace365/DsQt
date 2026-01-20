pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import QtMultimedia
import QtWebEngine
import QtQuick.Pdf
import QtQuick.VectorImage

// An element that can display images, videos, web content and PDFs.
Item {
    id: root

    // The media resource, containing file path and crop information, among other things.
    property var media: undefined
    // The file path of the media as reported by the CMS.
    readonly property string source: media && media.filepath ? media.filepath : ""
    // Preferred fill mode.
    property int fillMode: Image.PreserveAspectCrop
    // Preferred loop mode.
    property int loops: 0
    // auto play time based media
    property bool autoPlay: true
    // pdf page number
    property int page: 1
    // type of content to display. if empty, will determine based on file extension
    property string contentType: "" // "image", "image sequence", "web", "pdf", "video", "video stream","youtube?"
    // Contains the media dimensions as reported by the CMS.
    readonly property real mediaWidth: media && media.width ? media.width : 0
    readonly property real mediaHeight: media && media.height ? media.height : 0
    // Contains the cropping rectangle as reported by the CMS.
    readonly property real cropX: media && media.crop ? media.crop[0] : 0
    readonly property real cropY: media && media.crop ? media.crop[1] : 0
    readonly property real cropW: media && media.crop ? media.crop[2] : 1
    readonly property real cropH: media && media.crop ? media.crop[3] : 1
    readonly property bool isCropped: cropX > 0 || cropY > 0 || cropW < 1 || cropH < 1
    // If true, does not perform cropping but shows a cropping overlay instead.
    property bool cropOverlay: false

    //component array for controllers - we may not need these as we can make controllers and connect them
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

    //SIGNALS
    signal mediaLoaded()
    signal videoFinished()

    // TODO add additional signals for image load, video start, etc.

    // TODO add media specific controls

    // Function to determine the component based on file extension
    function getComponentForExtension() {
        if(source) {
            var ext = source.toLowerCase().split('.').pop();
            if (['mp4', 'avi', 'mov', 'mkv','webm'].indexOf(ext) !== -1) {
                return videoComponent;
            } else if (['jpg', 'jpeg', 'png', 'gif', 'bmp','webp'].indexOf(ext) !== -1) {
                return isCropped ? imageComponent : animatedImageComponent;
            } else if (['svg', 'lottie'].indexOf(ext) !== -1) {
                return vectorComponent;
            } else if (['html', 'htm'].indexOf(ext) !== -1 || source.startsWith("http")) {
                return webViewComponent;
            } else if (['pdf'].indexOf(ext) !== -1) {
                return pdfViewComponent;
            } else {
                console.log("Unsupported file extension: " + ext);
            }
        }
        return null;
    }

    function getComponentForContent() {
        let ctype = contentType.toLowerCase().trim();
        if(!ctype){
            ctype = media && media.type ? media.type : "";
        }

        if(ctype === "video" || ctype === "video stream" ) {
            return videoComponent;
        } else if(ctype === "image") {
            return isCropped ? imageComponent : animatedImageComponent;
        } else if(ctype === "image sequence") {
            return animatedImageComponent;
        } else if(ctype === "vector" || ctype === "vector sequence") {
            return vectorComponent;
        } else if(ctype === "web" || ctype === "youtube") {
            return webViewComponent;
        } else if(ctype === "pdf") {
            return pdfViewComponent;
        } else {
            return getComponentForExtension();
        }
    }

    // Calculates the actual rendered width of the item based on its fill mode.
    function getRenderedWidth(item) {
        if (item.fillMode === Image.Stretch) return item.width
        let aspectRatio = item.implicitWidth / item.implicitHeight
        if (item.fillMode === Image.PreserveAspectFit) {
            Math.min(item.width, item.height * aspectRatio)
        } else if(item.fillMode === Image.PreserveAspectCrop) {
            Math.max(item.width, item.height * aspectRatio)
        }
        return item.width
    }

    // Calculates the actual rendered height of the item based on its fill mode.
    function getRenderedHeight(item) {
        if (item.fillMode === Image.Stretch) return item.height
        let aspectRatio = item.implicitWidth / item.implicitHeight
        if (item.fillMode === Image.PreserveAspectFit) {
            return Math.min(item.height, item.width / aspectRatio)
        } else if(item.fillMode === Image.PreserveAspectCrop) {
            return Math.max(item.height, item.width / aspectRatio)
        }
        return item.height
    }

    // Loader to dynamically load the appropriate component
    Loader {
        id: contentLoader
        anchors.fill: parent
        sourceComponent: root.getComponentForContent()
        asynchronous: true
    }

    // Crop indicator. TODO maybe use a component, so we only construct this if needed?
    Item {
        id: overlay
        anchors.fill: contentLoader
        visible: root.cropOverlay && (root.cropX !== 0 || root.cropY !== 0 || root.cropW !== 1 || root.cropH !== 1)

        // Use properties from the loaded item (Video or Image)
        readonly property real mediaWidth: contentLoader.item ? contentLoader.item.renderedWidth : width
        readonly property real mediaHeight: contentLoader.item ? contentLoader.item.renderedHeight : height
        readonly property real offsetX: contentLoader.item ? contentLoader.item.renderedOffsetX : 0
        readonly property real offsetY: contentLoader.item ? contentLoader.item.renderedOffsetY : 0

        Shape {
            id: shape
            x: overlay.offsetX
            y: overlay.offsetY
            width: overlay.mediaWidth
            height: overlay.mediaHeight

            ShapePath {
                fillColor: "#80000000"
                strokeWidth: 0

                PathSvg {
                    readonly property real cx: cropX * shape.width
                    readonly property real cy: cropY * shape.height
                    readonly property real cw: cropW * shape.width
                    readonly property real ch: cropH * shape.height
                    path: "M0,0h"+shape.width+"v"+shape.height+"h"+(-shape.width)+"z"+"m"+cx+","+cy+"v"+ch+"h"+cw+"v"+(-ch)+"z"
                }
            }

            ShapePath {
                fillColor: "transparent"
                strokeColor: "white"
                strokeWidth: 1

                PathSvg {
                    readonly property real cx: cropX * shape.width
                    readonly property real cy: cropY * shape.height
                    readonly property real cw: cropW * shape.width
                    readonly property real ch: cropH * shape.height
                    path: "M"+cx+","+cy+"v"+ch+"h"+cw+"v"+(-ch)+"z"
                }
            }
        }
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

            property bool loading: true
            onBufferProgressChanged: {
                if(loading && video.bufferProgress >= 1) {
                    loading = false
                    root.mediaLoaded()
                    console.log("Video loaded: ", video.source)
                }
            }

            onStopped: root.videoFinished()

            readonly property real renderedOffsetX: (width - renderedWidth) / 2
            readonly property real renderedOffsetY: (height - renderedHeight) / 2
            readonly property real renderedWidth: root.getRenderedWidth(video)
            readonly property real renderedHeight: root.getRenderedHeight(video)
        }
    }

    // Component for (cropped) Image
    Component {
        id: imageComponent

        Image {
            id: image
            anchors.fill: parent
            source: root.source
            fillMode: root.fillMode
            retainWhileLoading: true
            visible: root.isCropped

            sourceSize.width: root.mediaWidth
            sourceSize.height: root.mediaHeight
            sourceClipRect: root.cropOverlay ? undefined : Qt.rect(root.cropX * root.mediaWidth, root.cropY * root.mediaHeight, root.cropW * root.mediaWidth, root.cropH * root.mediaHeight)

            onStatusChanged: {
                if (status === Image.Error) {
                    console.log("Image loading error")
                }
            }

            property bool loading: true
            onProgressChanged: {
                if (loading && image.progress >= 1) {
                    loading = false
                    root.mediaLoaded()
                    console.log("Image loaded: ", image.source)
                }
            }

            readonly property real renderedOffsetX: (width - renderedWidth) / 2
            readonly property real renderedOffsetY: (height - renderedHeight) / 2
            readonly property real renderedWidth: root.getRenderedWidth(image)
            readonly property real renderedHeight: root.getRenderedHeight(image)
        }
    }

    // Component for (animated) Image
    Component {
        id: animatedImageComponent

        AnimatedImage {
            id: animatedImage
            anchors.fill: parent
            source: root.source
            fillMode: root.fillMode
            retainWhileLoading: true
            playing: false
            visible: !root.isCropped

            onStatusChanged: {
                if (status === AnimatedImage.Error) {
                    console.log("AnimatedImage loading error")
                }
                playing = (status == AnimatedImage.Ready) && root.autoPlay
            }

            property bool loading: true
            onProgressChanged: {
                if (loading && animatedImage.progress >= 1) {
                    loading = false
                    root.mediaLoaded()
                    console.log("AnimatedImage loaded: ", animatedImage.source)
                }
            }

            readonly property real renderedOffsetX: (width - renderedWidth) / 2
            readonly property real renderedOffsetY: (height - renderedHeight) / 2
            readonly property real renderedWidth: root.getRenderedWidth(animatedImage)
            readonly property real renderedHeight: root.getRenderedHeight(animatedImage)
        }
    }

    // Component for VectorImage
    Component {
        id: vectorComponent

        VectorImage {
            id: vectorImage
            preferredRendererType: VectorImage.CurveRenderer
            source: root.source
            fillMode: root.fillMode === Image.PreserveAspectFit ? VectorImage.PreserveAspectFit : root.fillMode === Image.PreserveAspectCrop ? VectorImage.PreserveAspectCrop : undefined

            property bool loading: true
            Component.onCompleted: {
                if (status === VectorImage.Ready) {
                    if(loading){
                        loading = false
                        root.mediaLoaded()
                    }
                }
            }

            readonly property real renderedOffsetX: (width - renderedWidth) / 2
            readonly property real renderedOffsetY: (height - renderedHeight) / 2
            readonly property real renderedWidth: root.getRenderedWidth(vectorImage)
            readonly property real renderedHeight: root.getRenderedHeight(vectorImage)
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
                } else if(loadingInfo.status === WebEngineView.LoadSucceededStatus) {
                    root.mediaLoaded()
                }
            }

            // Note: root.fillMode is ignored.

            readonly property real renderedOffsetX: (width - renderedWidth) / 2
            readonly property real renderedOffsetY: (height - renderedHeight) / 2
            readonly property real renderedWidth: root.getRenderedWidth(webView)
            readonly property real renderedHeight: root.getRenderedHeight(webView)
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

                property bool loading: true
                Component.onCompleted: {
                    if (pdfDoc.pageCount > 0) {
                        pdfView.goToPage(root.page-1);

                        fitToView()

                        if(loading) {
                            loading = false
                            root.mediaLoaded()
                        }
                    }
                }

                onCurrentPageChanged: fitToView()

                function fitToView() {
                    if(root.fillMode === Image.PreserveAspectFit) {
                        pdfView.scaleToPage(pdfContainer.width,pdfContainer.height)
                    }
                    else if(root.fillMode === Image.PreserveAspectCrop) {
                        pdfView.scaleToWidth(pdfContainer.width, pdfContainer.height)
                    }
                }
            }

            readonly property real renderedOffsetX: (width - renderedWidth) / 2
            readonly property real renderedOffsetY: (height - renderedHeight) / 2
            readonly property real renderedWidth: root.getRenderedWidth(pdfView)
            readonly property real renderedHeight: root.getRenderedHeight(pdfView)
        }
    }
}
