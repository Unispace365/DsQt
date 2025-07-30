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
    title: `${Application.name} (${Application.version})`
    flags: Qt.FramelessWindowHint | Qt.Window
    property string mode:"normal"
    ///The path to the QML object to load as the root
    property url rootSrc:"";


    Item {
        id: root
        width: window.width;
        height: window.height;

    Shortcut {
        sequences: ["ESCAPE","CTRL+Q"]
        autoRepeat: false;
        onActivated: Qt.quit();
        context: Qt.ApplicationShortcut
    }

    Component.onCompleted: {
    }
    


    DsSettingsProxy {
        id:appProxy
        target:"app_settings"
    }

    DsSettingsProxy {
        id:windowProxy
        target:"engine"
        prefix: "engine.window"
    }


    DsScaleLoader {
       width: windowProxy.getRect("source").width
       height: windowProxy.getRect("source").height
       id: loadera
       rootSource: window.rootSrc;
    }

    Shortcut {
        sequence: "S"
        autoRepeat: false
        onActivated: {window.mode = window.mode != "scale" ? "scale" : "normal"}
        context: Qt.ApplicationShortcut
    }

    Shortcut {
        sequence: "T"
        autoRepeat: false
        onActivated: {window.mode = window.mode != "translate" ? "translate" : "normal" }
        context: Qt.ApplicationShortcut
    }

    Shortcut {
        sequence: "0"
        autoRepeat: false
        onActivated: {
            window.mode = "normal"
            loadera.viewScale = 1.0;
            loadera.viewPos = Qt.point(0,0);
            loadera.scaleView()
        }
        context: Qt.ApplicationShortcut
    }

    MouseArea {
        enabled: {window.mode != "normal"}
        width: window.width;
        height: window.height;
        property real startX
        property real startY
        onPressed: {
            startX = mouseX;
            startY = mouseY;
        }
        onMouseXChanged: {
            if(window.mode == "scale"){
                if(window.width>0){
                    let dist = (mouseX - startX)/window.width*2
                    loadera.viewScale += dist;
                    if(loadera.viewScale<0.25){
                        loadera.viewScale = 0.25
                    }

                    loadera.scaleView();
                    startX = mouseX;
                }
            } else if(window.mode == "translate"){
                if(window.width>0){
                    let distx = (startX - mouseX)/loadera.viewScale
                    let disty = (startY - mouseY)/loadera.viewScale
                    loadera.viewPos.x += distx;
                    loadera.viewPos.y += disty;


                    loadera.scaleView();
                    startX = mouseX;
                    startY = mouseY;
                }
            }

        }
    }

    Text {
        anchors.horizontalCenter: root.horizontalCenter
        anchors.verticalCenter: root.verticalCenter
        visible: window.mode != "normal"
        font.pixelSize: 56
        opacity: 0.25
        color: "white"
        font.capitalization: Font.Capitalize
        text: window.mode
    }


    Connections {
        target: Ds.engine
        function onResetting(path) {
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
}
