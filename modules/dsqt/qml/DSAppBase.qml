import QtQuick
import QtQuick.VirtualKeyboard
import dsqt
/// \brief main window for DsQt QML applications.
/// Takes a url to a qml file to use as the world root.
/// \ingroup QML
Window {
    id: window
    width: windowProxy.getRect("destination").width;
    height:  windowProxy.getRect("destination").height;
    visible: true
    title: qsTr("Hello World")
    flags: Qt.FramelessWindowHint | Qt.Window
    ///The path to the QML object to load as the root
    property url rootSrc:"";
    Shortcut {
        sequences: ["ESCAPE","CTRL+Q"]
        autoRepeat: false;
        onActivated: Qt.quit();
        context: Qt.ApplicationShortcut
    }


    Component.onCompleted: {
    }


    DSSettingsProxy {
        id:appProxy
        target:"app_settings"
    }

    DSSettingsProxy {
        id:windowProxy
        target:"engine"
        prefix: "engine.window"
    }

    Loader {
       id: loader
       property alias scale_origin: loaderScale.origin
       property alias xScale: loaderScale.xScale;
       property alias yScale: loaderScale.yScale;
       property alias xTrans: loaderTrans.x;
       property alias yTrans: loaderTrans.y;

       width: windowProxy.getSize("world_dimensions").width;
       height: windowProxy.getSize("world_dimensions").height;
       source: rootSrc;
       transform: [
            Scale {
               id: loaderScale
               origin.x:0
               origin.y:0
               xScale: 1;
               yScale: 1;
            },
            Translate {
                  id: loaderTrans
            }
       ]
       Component.onCompleted: {
           //resize the loader to fit in the destination
           var src = windowProxy.getRect("source");
           var dst = Qt.size(window.width,window.height);
           var x_scale = dst.width/src.width
           var y_scale = dst.height/src.height
           var offset = Qt.point(src.x+0.5*src.width,src.y+0.5*src.height);
           console.log("scale: "+x_scale+","+y_scale);
           console.log("offset "+offset.x+","+offset.y);

           loader.x = -offset.x + 0.5*dst.width;
           loader.y = -offset.y + 0.5*dst.height;
           loader.scale_origin.x = offset.x;
           loader.scale_origin.y = offset.y;
           loader.xScale = x_scale;
           loader.yScale = y_scale;
           //loader.xTrans = -offset.x + 0.5*dst.width;
           //loader.yTrans = -offset.y + 0.5*dst.height;

       }
    }

    Imgui {
        id: imgui
        objectName: "imgui"
        anchors.fill: parent
        SequentialAnimation on opacity {
            id: opacityAnim
            running: false
            NumberAnimation { from: 1; to: 0; duration: 3000 }
            NumberAnimation { from: 0; to: 1; duration: 3000 }
        }
    }
}
