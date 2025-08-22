import QtQuick
import Dsqt



Loader {
    id: loadera
    //asynchronous: true

    property alias scale_origin: loaderScale.origin
    property alias xScale: loaderScale.xScale;
    property alias yScale: loaderScale.yScale;
    property alias xTrans: loaderTrans.x;
    property alias yTrans: loaderTrans.y;
    property real viewScale: 1.0;
    property point viewPos: Qt.point(0,0);
    property url rootSource: ""
    source: rootSource

    DsSettingsProxy {
        id:windowProxy
        target:"engine"
        prefix: "engine.window"
    }

    width: windowProxy.getSize("world_dimensions").width;
    height: windowProxy.getSize("world_dimensions").height;

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

    function reload() {
        console.log("reloading")
        loadera.source = "";
        //loadera.asynchronous = false;
        Ds.engine.clearQmlCache();
        //loadera.source = rootSrc;
        // loadera.asynchronous = true;
        //$QmlEngine.clearQmlCache();
        //loadera.source = rootSrc;
        timeroo.start()
    }

    Timer {
        id: timeroo
        interval: 1000
        running: false
        repeat: false
        onTriggered: {
            Ds.engine.clearQmlCache();
            loadera.source = loadera.rootSource+"?tip="+Math.random()
        }
    }

    onStatusChanged: console.log(status)
    //onLoaded: loadera.visible = true;

    function scaleView(scale: real, pos: point){
        var x_scale = loadera.viewScale ?? 1.0
        var y_scale = loadera.viewScale ?? 1.0

        // Resize the loader to fit in the destination.
        var preferredWidth = windowProxy.getInt("width", 0);
        var preferredHeight = windowProxy.getInt("height", 0);
        var availableWidth = Window.width;
        var availableHeight = Window.height;

        if(preferredWidth > 0) x_scale *= availableWidth / preferredWidth;
        if(preferredHeight > 0)  y_scale *= availableHeight / preferredHeight;

        var offset = Qt.point(0.5 * preferredWidth, 0.5 * preferredHeight);
        offset.x += loadera.viewPos?.x ?? 0;
        offset.y += loadera.viewPos?.y ?? 0;

        loadera.x = -offset.x + 0.5 * availableHeight;
        loadera.y = -offset.y + 0.5 * availableHeight;

        loadera.scale_origin.x = offset.x;
        loadera.scale_origin.y = offset.y;

        loadera.xScale = x_scale;
        loadera.yScale = y_scale;

        //loader.xTrans = -offset.x + 0.5*availableHeight;
        //loader.yTrans = -offset.y + 0.5*availableHeight;
    }

    Component.onCompleted: {
        scaleView(1.0,Qt.point(0,0));
    }





}
