import QtQuick
import QtQuick.Controls
import QtMultimedia
//import QtWebView
import Dsqt

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
    //
    property bool autoPlay: true

    signal videoFinished()

    // TODO add additional signals for image load, video start, etc.

    // TODO add media specific controls

    // Function to determine the component based on file extension
    function getComponentForExtension() {
        var ext = source.toLowerCase().split('.').pop();
        if (['mp4', 'avi', 'mov', 'mkv'].indexOf(ext) !== -1) {
            return videoComponent;
        } else if (['jpg', 'jpeg', 'png', 'gif', 'bmp'].indexOf(ext) !== -1) {
            return imageComponent;
        } else if (['html', 'htm'].indexOf(ext) !== -1 || source.startsWith("http")) {
            return webViewComponent;
        } else {
            console.log("Unsupported file extension: " + ext);
            return null;
        }
    }

    // Loader to dynamically load the appropriate component
    Loader {
        id: contentLoader
        anchors.fill: parent
        sourceComponent: getComponentForExtension()
        asynchronous: true
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
        Image {
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
