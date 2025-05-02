import QtQuick
import Dsqt
import QtMultimedia


Item {
    id:sizer
    Component.onCompleted: {
        console.log("platform id is "+BridgeUtility.platform["uid"])
        console.log("Moment is "+moment().format("YYYY-MM-DDTHH:mm:ss"))
        console.log("Sizer size:"+sizer.width+","+sizer.height)
        var events = BridgeUtility.getEventsForSpan("1970-01-01T00:00:00","2088-01-01T00:00:00");
        console.log("events size:"+events.length);
    }

    Rectangle {
        id:baseRect
        anchors.fill: parent
        color: "#0000ff"
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
            y:sizer.height*0.5+20;
            x:modelData*150
            width:contentWidth
            height:contentHeight
            text:""+modelData*150
            font.pixelSize: 50
            color: "#000000"
        }
    }

    /*SequentialAnimation on x {
        id: anim
        property real initScale: 0.5;
        loops: Animation.Infinite
        NumberAnimation { to: sizer.width; duration: 10000 * anim.initScale; onStopped: { anim.initScale=1; }}
        NumberAnimation { to: -sizer.width; duration: 10000 }
        Component.onCompleted: {
            anim.initScale = 1.0 - (x-(-sizer.width)/(sizer.width - (-sizer.width)));
            console.log(anim.initScale)
        }
    }*/

    // DSRadialMenu {
    //     id: radialMenu
    //     radius: 250
    //     width: 500
    //     height: 500
    //     model: [
    //         {
    //             text: "Menu",
    //             icon: "qrc:/res/data/waffles/icons/1x/Home_64.png",
    //             action: function() {
    //                 console.log("Menu clicked");
    //             }
    //         },
    //         {
    //             text: "Settings",
    //             icon: "qrc:/icons/settings.svg",
    //             action: function() {
    //                 console.log("Settings clicked");
    //             }
    //         },
    //         {
    //             text: "what",
    //             icon: "qrc:/icons/close.svg",
    //             action: function() {
    //                 console.log("Close clicked");
    //             }
    //         }
    //     ]
    //     delegate: DSRadialMenuIconItem {
    //         text: modelData.text
    //     }
    //     DragHandler {
    //         target: radialMenu
    //     }
    // }

    /*MultiPointTouchArea {
        id: touchArea
        anchors.fill: parent
        minimumTouchPoints: 5
        maximumTouchPoints: 5
        mouseEnabled: true
        onPressed: {
            console.log("onPressed")
            //anim.running = true
        }
        onReleased: {
            console.log("onReleased")
            //anim.running = false
        }
    }*/

    ClusterView {
        id: jawa
        manager: wookie

        DSRadialMenu {
            id: radialMenu
            radius: 250
            width: 500
            height: 500
            visible: false
            state: "Start"
            model: [
                {
                    text: "Menu",
                    icon: "qrc:/res/data/waffles/icons/1x/Home_64.png",
                    action: function() {
                        console.log("Menu clicked");
                    }
                },
                {
                    text: "Settings",
                    icon: "qrc:/icons/settings.svg",
                    action: function() {
                        console.log("Settings clicked");
                    }
                },
                {
                    text: "what",
                    icon: "qrc:/icons/close.svg",
                    action: function() {
                        console.log("Close clicked");
                    }
                }
            ]
            delegate: DSRadialMenuIconItem {
                property int index
                text: modelData.text
            }
            DragHandler {
                target: radialMenu
            }


        }
    }


    ClusterManager {
        id: wookie
        enabled:true;
        minClusterTouchCount: 3
        minClusterSeperation: 500
        anchors.fill: parent
    }

}
