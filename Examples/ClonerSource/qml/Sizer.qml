import QtQuick
import Dsqt
import QtMultimedia


Item {
    id:sizer
    Component.onCompleted: {
        //console.log("platform id is "+BridgeUtility.platform["uid"])
        //console.log("Moment is "+moment().format("YYYY-MM-DDTHH:mm:ss"))
        //console.log("Sizer size:"+sizer.width+","+sizer.height)
        //var events = BridgeUtility.getEventsForSpan("1970-01-01T00:00:00","2088-01-01T00:00:00");
        //console.log("events size:"+events.length);
    }

    Rectangle {
        id:baseRect
        anchors.fill: parent
        color: "#ffffff"
        enabled: false;
    }

    Rectangle {
        x:baseRect.x+20;
        y:baseRect.y+20;
        width:baseRect.width-40;
        height:baseRect.height-40;
        color: "#80ff0000"
    }

    Rectangle {
        y:sizer.height*0.5;
        x:0
        width: sizer.width;
        height: 20;
        color: "#000000"
    }

    Repeater {
        id: repline
        model: sizer.width/150.0

        Text {
            required property var modelData
            visible: !Ds.engine.idle.idling
            y:sizer.height*0.5+20;
            x:modelData*150
            width:contentWidth
            height:contentHeight
            text:""+modelData*150
            font.pixelSize: 50
            color: "#000000"
        }
    }

    DsSettingsProxy {
        id:appProxy
        target:"app_settings"
    }

    DsClock {
        id: clock
        // interval: 50
        // speed: 1800
        anchors.left: parent.left
        anchors.leftMargin: 20
        anchors.top: parent.top
        anchors.topMargin: 20
        width: 150
        height: 150
        secondHandColor: "#00000000"
    }

    DsEventSchedule {
        id: contentEvents
        clock: clock
    }

    DsClusterView {
        id: jawa
        manager: wookie
        menuConfig: appProxy.getObj("menu")
        menuModel: appProxy.getList("menu.item")

        DsQuickMenu {
        }
    }

    DsClusterManager {
        id: wookie
        enabled:true;
        minClusterTouchCount: 3
        minClusterSeperation: 500
        holdOpenOnTouch: true
        anchors.fill: parent
    }

    Connections {
        target: Ds.engine
        function onRootUpdated() {
            //let m = Ds.getEventsForSpan(new Date().toISOString(),new Date().toISOString());
            //console.log("***----------->",m)
            //console.log("root updated in qml")
            //let ev = Ds.getEventsForSpan(new Date().toISOString(),new Date().toISOString())[0]
            //if(playlist.model !== ev){

                //playlist.model = ev

            //}
        }
    }
}
