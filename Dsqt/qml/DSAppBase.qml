import QtQuick
import QtQuick.VirtualKeyboard
import Dsqt

/// \brief main window for DsQt QML applications.
/// Takes a url to a qml file to use as the world root.
/// \ingroup QML
Window {
    id: window
    x: windowProxy.getBool("center")?(Screen.width- width)/2:windowProxy.getRect("destination").x;
    y: windowProxy.getBool("center")?(Screen.height - height)/2:windowProxy.getRect("destination").y;
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


    DSScaleLoader {
       id: loadera
       source: rootSrc;
    }

    Connections {
        target: $QmlEngine
        function onFileChanged(path) {
            console.log("file changed qml");
            loadera.reload();
        }

    }


    /*Imgui {
        id: imgui
        objectName: "imgui"
        anchors.fill: parent
        SequentialAnimation on opacity {
            id: opacityAnim
            running: false
            NumberAnimation { from: 1; to: 0; duration: 3000 }
            NumberAnimation { from: 0; to: 1; duration: 3000 }
        }
    }*/
}
