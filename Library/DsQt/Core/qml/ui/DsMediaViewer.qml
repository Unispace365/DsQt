pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import QtQuick.VectorImage
import Dsqt.Core

// An element that can display images, videos, web content and PDFs.
Item {
    id: root

    // The media resource, containing file path and crop information, among other things.
    property var media: undefined
    // The file path of the media as reported by the CMS (raw; used for type/extension detection).
    readonly property string source: media && media.filepath ? media.filepath : ""
    // The source resolved to a loadable URL: network URLs (http/https) pass through, while local
    // and %VAR%-prefixed paths are expanded into proper file URLs. This is what the actual media
    // elements load, so both local and network images/videos work.
    readonly property url resolvedSource: mediaType !== "capture" && source !== "" ? Ds.env.expandUrl(source) : ""
    // The currently loaded content item (video/web/pdf/image element); lets controls drive it.
    readonly property var mediaItem: contentLoader.item
    // Resolved media kind ("video" | "image" | "web" | "pdf" | "vector" | ""), from the explicit
    // contentType, else the model type, else the file extension. Drives which controls to show.
    readonly property string mediaType: {
        let ct = contentType.toLowerCase().trim()
        if (!ct) ct = (media && media.type) ? String(media.type).toLowerCase() : ""
        if (ct === "video" || ct === "video stream") return "video"
        if (ct === "capture") return "capture"
        if (ct === "image" || ct === "image sequence") return "image"
        if (ct === "vector" || ct === "vector sequence") return "vector"
        if (ct === "web" || ct === "youtube") return "web"
        if (ct === "pdf") return "pdf"
        if (source) {
            let ext = source.toLowerCase().split('.').pop()
            if (['mp4','avi','mov','mkv','webm'].indexOf(ext) !== -1) return "video"
            if (['jpg','jpeg','png','gif','bmp','webp'].indexOf(ext) !== -1) return "image"
            if (['svg','lottie'].indexOf(ext) !== -1) return "vector"
            if (['html','htm'].indexOf(ext) !== -1 || source.startsWith("http")) return "web"
            if (ext === 'pdf') return "pdf"
        }
        return ""
    }
    // Video transport handle: the loaded item when it is a video (exposes position/duration/
    // playbackState/volume/loops + play/pause/seek), otherwise null. Lets controls drive playback.
    readonly property bool isVideo: contentLoader.item ? ("playbackState" in contentLoader.item) : false
    readonly property var videoItem: isVideo ? contentLoader.item : null
    // A backend-neutral status contract. Presentation and recovery controls belong to the host.
    readonly property string loadState: _loadState // empty | loading | ready | error
    readonly property bool loading: loadState === "loading"
    readonly property string errorString: _errorString
    property string _loadState: "empty"
    property string _errorString: ""
    property bool _acceptStatus: false
    // Preferred fill mode.
    property int fillMode: Image.PreserveAspectCrop
    // Preferred loop mode.
    property int loops: 1
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

    // Lazy-loaded components from Dsqt.Waffles (resolved at runtime)
    property Component _videoComponent: null
    property Component _captureComponent: null
    property Component _webComponent: null
    property Component _pdfComponent: null

    function _lazyLoad(prop, typeName) {
        if (root[prop] === null) {
            let c = Qt.createComponent("Dsqt.Waffles", typeName)
            if (c && c.status === Component.Ready) {
                root[prop] = c
            } else {
                console.warn("DsMediaViewer: Could not load", typeName, "from Dsqt.Waffles.",
                             c ? c.errorString() : "Module not available")
                return null
            }
        }
        return root[prop]
    }

    onMediaChanged: _scheduleReload()
    onResolvedSourceChanged: _scheduleReload()
    onContentTypeChanged: _scheduleReload()
    onIsCroppedChanged: _scheduleReload()
    Component.onCompleted: _scheduleReload()

    function _scheduleReload() {
        // Coalesce dependent bindings and ignore signals from the outgoing backend.
        _acceptStatus = false;
        Qt.callLater(root._reload);
    }

    function retry() { _scheduleReload(); }

    function _reload() {
        contentLoader.sourceComponent = null;
        _errorString = "";
        _loadState = "empty";
        if (!source && mediaType !== "capture") return;
        _loadState = "loading";
        _acceptStatus = true;
        const component = getComponentForContent();
        if (!component) {
            _failed(qsTr("This type of content is not available in this application."));
            return;
        }
        contentLoader.sourceComponent = component;
    }

    function _ready() {
        if (!_acceptStatus || _loadState === "ready") return;
        _errorString = "";
        _loadState = "ready";
        mediaLoaded();
    }

    function _failed(detail) {
        if (!_acceptStatus) return;
        _errorString = detail || qsTr("The content could not be loaded.");
        _loadState = "error";
        console.warn("DsMediaViewer:", source, _errorString);
        mediaFailed(_errorString);
    }

    //SIGNALS
    signal mediaLoaded()
    signal mediaFailed(string detail)
    signal videoFinished()

    // TODO add additional signals for image load, video start, etc.

    // TODO add media specific controls

    // Function to determine the component based on file extension
    function getComponentForExtension() {
        if(source) {
            var ext = source.toLowerCase().split('.').pop();
            if (['mp4', 'avi', 'mov', 'mkv','webm'].indexOf(ext) !== -1) {
                return _lazyLoad("_videoComponent", "DsVideoViewer");
            } else if (['jpg', 'jpeg', 'png', 'gif', 'bmp','webp'].indexOf(ext) !== -1) {
                return isCropped ? imageComponent : animatedImageComponent;
            } else if (['svg', 'lottie'].indexOf(ext) !== -1) {
                return vectorComponent;
            } else if (['html', 'htm'].indexOf(ext) !== -1 || source.startsWith("http")) {
                return _lazyLoad("_webComponent", "DsWebViewer");
            } else if (['pdf'].indexOf(ext) !== -1) {
                return _lazyLoad("_pdfComponent", "DsPdfViewer");
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

        if(ctype === "capture") {
            return _lazyLoad("_captureComponent", "DsCaptureViewer");
        } else if(ctype === "video" || ctype === "video stream" ) {
            return _lazyLoad("_videoComponent", "DsVideoViewer");
        } else if(ctype === "image") {
            return isCropped ? imageComponent : animatedImageComponent;
        } else if(ctype === "image sequence") {
            return animatedImageComponent;
        } else if(ctype === "vector" || ctype === "vector sequence") {
            return vectorComponent;
        } else if(ctype === "web" || ctype === "youtube") {
            return _lazyLoad("_webComponent", "DsWebViewer");
        } else if(ctype === "pdf") {
            return _lazyLoad("_pdfComponent", "DsPdfViewer");
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

    // Assign imperatively: lazy component caching would cause a sourceComponent binding loop.
    Loader {
        id: contentLoader
        anchors.fill: parent
        asynchronous: true
        onStatusChanged: if (status === Loader.Error) root._failed(qsTr("The viewer could not be opened."))
        onLoaded: {
            if (item) {
                // Connect before assigning a source: local assets may finish synchronously.
                const backend = item;
                if ('mediaLoaded' in item) item.mediaLoaded.connect(() => {
                    if (contentLoader.item === backend) root._ready();
                })
                if ('mediaFailed' in item) item.mediaFailed.connect(detail => {
                    if (contentLoader.item === backend) root._failed(detail);
                })
                if ('videoFinished' in item) item.videoFinished.connect(() => {
                    if (contentLoader.item === backend) root.videoFinished();
                })
                if ('captureMedia' in item) item.captureMedia = Qt.binding(() => root.media || ({}))
                if ('fillMode' in item) item.fillMode = Qt.binding(() => root.fillMode)
                if ('loops' in item) item.loops = Qt.binding(() => root.loops)
                if ('autoPlay' in item) item.autoPlay = Qt.binding(() => root.autoPlay)
                if ('page' in item) item.page = Qt.binding(() => root.page)
                if ('source' in item) item.source = Qt.binding(() => root.resolvedSource)
                if ('errorString' in item && item.errorString) root._failed(item.errorString)
                else if ('ready' in item && item.ready) root._ready()
            }
        }
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

    // Component for (cropped) Image
    Component {
        id: imageComponent

        Image {
            id: image
            anchors.fill: parent
            source: root.resolvedSource
            fillMode: root.fillMode
            retainWhileLoading: true
            visible: root.isCropped

            sourceSize.width: root.mediaWidth
            sourceSize.height: root.mediaHeight
            sourceClipRect: root.cropOverlay ? undefined : Qt.rect(root.cropX * root.mediaWidth, root.cropY * root.mediaHeight, root.cropW * root.mediaWidth, root.cropH * root.mediaHeight)

            onStatusChanged: {
                if (status === Image.Ready) root._ready()
                else if (status === Image.Error) root._failed(qsTr("The image could not be loaded."))
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
            source: root.resolvedSource
            fillMode: root.fillMode
            retainWhileLoading: true
            playing: false
            visible: !root.isCropped

            onStatusChanged: {
                if (status === AnimatedImage.Ready) root._ready()
                else if (status === AnimatedImage.Error) root._failed(qsTr("The image could not be loaded."))
                playing = (status == AnimatedImage.Ready) && root.autoPlay
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
            source: root.resolvedSource
            fillMode: root.fillMode === Image.PreserveAspectFit ? VectorImage.PreserveAspectFit : root.fillMode === Image.PreserveAspectCrop ? VectorImage.PreserveAspectCrop : undefined

            onStatusChanged: {
                if (status === VectorImage.Ready) root._ready()
                else if (status === VectorImage.Error) root._failed(qsTr("The illustration could not be loaded."))
            }

            readonly property real renderedOffsetX: (width - renderedWidth) / 2
            readonly property real renderedOffsetY: (height - renderedHeight) / 2
            readonly property real renderedWidth: root.getRenderedWidth(vectorImage)
            readonly property real renderedHeight: root.getRenderedHeight(vectorImage)
        }
    }
}
