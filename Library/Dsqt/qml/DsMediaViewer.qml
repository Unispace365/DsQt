import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtMultimedia
//import QtWebView
import Dsqt

// An element that can display images, videos, web content and PDFs.
Item {
    id: root

    // The media content item, containing file path and crop information, among other things.
    property var media: undefined
    // Preferred fill mode.
    property int fillMode: Image.PreserveAspectCrop
    // Preferred loop mode.
    property int loops: 0
    //
    property bool autoPlay: true
    // The file path of the media as reported by the CMS.
    readonly property string source: media ? media.filepath : undefined
    // Contains the media dimensions as reported by the CMS.
    readonly property real mediaWidth: media && media.width ? media.width : undefined
    readonly property real mediaHeight: media && media.height ? media.height : undefined
    // Contains the cropping rectangle as reported by the CMS.
    readonly property real cropX: media && media.crop ? media.crop[0] : 0
    readonly property real cropY: media && media.crop ? media.crop[1] : 0
    readonly property real cropW: media && media.crop ? media.crop[2] : 1
    readonly property real cropH: media && media.crop ? media.crop[3] : 1
    // If true, does not perform cropping but shows a cropping overlay instead.
    property bool cropOverlay: false

    // Signals.
    signal mediaLoaded()
    signal videoFinished()

    // TODO add additional signals for image load, video start, etc.

    // TODO add media specific controls

    // Function to determine the component based on file extension
    function getComponentForExtension() {
        if(source) {
            var ext = source.toLowerCase().split('.').pop();
            if (['mp4', 'avi', 'mov', 'mkv'].indexOf(ext) !== -1) {
                return videoComponent;
            } else if (['jpg', 'jpeg', 'png', 'gif', 'bmp'].indexOf(ext) !== -1) {
                return imageComponent;
            } else if (['html', 'htm'].indexOf(ext) !== -1 || source.startsWith("http")) {
                return webViewComponent;
            } else {
                console.log("Unsupported file extension: " + ext);
            }
        }
        return null;
    }

    // Calculates the actual rendered width of the item based on its fill mode.
    function getRenderedWidth(item) {
        if (item.fillMode === Image.Stretch) return item.width
        let aspectRatio = item.implicitWidth / item.implicitHeight
        let parentAspectRatio = item.width / item.height
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
        let parentAspectRatio = item.width / item.height
        if (item.fillMode === Image.PreserveAspectFit) {
            return Math.min(item.height, item.width / aspectRatio)
        } else if(item.fillMode === Image.PreserveAspectCrop) {
            return Math.max(item.height, item.width / aspectRatio)
        }
        return item.height
    }

    // Loader to dynamically load the appropriate component.
    Loader {
        id: contentLoader
        anchors.fill: parent
        sourceComponent: getComponentForExtension()
        asynchronous: true
    }

    // Crop indicator. TODO maybe use a component, so we only construct this if needed?
    Item {
        id: overlay
        anchors.fill: contentLoader
        visible: cropOverlay && (cropX !== 0 || cropY !== 0 || cropW !== 1 || cropH !== 1)

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
            readonly property real renderedWidth: getRenderedWidth(video)
            readonly property real renderedHeight: getRenderedHeight(video)
        }
    }

    // Component for Image
    Component {
        id: imageComponent
        Image {
            id: image
            anchors.fill: parent
            source: root.source
            sourceSize.width: root.mediaWidth
            sourceSize.height: root.mediaHeight
            sourceClipRect: root.cropOverlay ? undefined : Qt.rect(root.cropX * root.mediaWidth, root.cropY * root.mediaHeight, root.cropW * root.mediaWidth, root.cropH * root.mediaHeight)
            fillMode: root.fillMode
            retainWhileLoading: true
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
            readonly property real renderedWidth: getRenderedWidth(image)
            readonly property real renderedHeight: getRenderedHeight(image)
        }
    }

    // // Component for WebView
    // Component {
    //     id: webViewComponent
    //     WebView {
    //         id: webView
    //         anchors.fill: parent
    //         url: root.source
    //         onLoadingChanged: {
    //             if (loadRequest.status === WebView.LoadFailedStatus) {
    //                 console.log("WebView loading error: " + loadRequest.errorString)
    //             }
    //         }
    //     }
    // }
}
