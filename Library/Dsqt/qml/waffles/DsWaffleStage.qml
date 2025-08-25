import QtQuick 2.15



Item {
    id: wafflesRoot
    property Component launcher: Qt.createComponent("menu_launcher/TestLauncher.qml")
    property Component viewer: Qt.createComponent("TitledMediaViewer.qml");
    property Component fullscreenController: Qt.createComponent("FullscreenController.qml")
    property Component presentationController: Qt.createComponent("PresentationController.qml")
    property bool menuShown: false

    Component.onCompleted: {
        createMenu()
    }

    TapHandler {

    }

    Item {
        id: background
        anchors.fill: wafflesRoot
    }
    Item {
        id: foreground
        anchors.fill: wafflesRoot
    }
    Item {
        id: topLayer
        anchors.fill: wafflesRoot
    }

    //functions && _privates
    QtObject {
        id: _private
        property var launcher: null
    }

    function createMenu() {
        if (launcher.status == Component.Ready)
            completeMenu()
        else
            launcher.statusChanged.connect(completeMenu)
    }

    function completeMenu() {
        if(launcher.status == Component.Ready) {
            _private.launcher = launcher.createObject(topLayer,{"opacity":1});
            if(_private.launcher == null)
            {
                console.log("Error creating menu");
            }
        } else if (launcher.status === Component.Error) {
            console.log("Error loading component:", launcher.errorString());
        }
    }

    function createViewer(viewerProps: var) {
        if (viewer.status == Component.Ready)
            completeViewer(viewerProps)
        else
            viewer.statusChanged.connect(()=>{completeViewer(viewerProps);})
    }

    function completeViewer() {
        if(launcher.status == Component.Ready) {
            let viewerInstance = viewer.createObject(topLayer,{ "x": 30, "y": 30,"opacity":1});
            if(viewerInstance == null)
            {
                console.log("Error creating viewer");
            }
        } else if (launcher.status === Component.Error) {
            console.log("Error loading component:", viewer.errorString());
        }
    }

    function openViewer(viewerProps: var){
        createViewer(viewerProps);
    }
    TapHandler {

        onTapped: (point,button)=>{
            if(tapCount == 2){
                _private.launcher.x = point.position.x
                _private.launcher.y = point.position.y
            }
        }
    }
}
