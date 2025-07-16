import QtQuick 2.15
Item {
    id: base
    property Component delegate;
    property var model;
    property real startAngle: 0;
    property real radius: 200;
    property string spacing: "equal" //fixed, equal
    property string style: "spoke" // spoke, curve, circle
    property real spacingValue: 20;


    Rectangle {
        id:background
        color: "white"
        opacity: 0.5
        anchors.horizontalCenter: base.horizontalCenter
        anchors.verticalCenter: base.verticalCenter
        width: base.radius * 2
        height: base.radius * 2
        radius: base.radius
    }

    Repeater {
        id: vizRepeater
        anchors.fill: parent
        model: base.model
        delegate: base.delegate
        onModelChanged: {

        }

        onItemAdded: (index, item) =>{
            item.index = index;
            //item.vizCount = vizRepeater.count;
            let len = item.width;
            let _spacing = base.spacing === "fixed" ? base.spacingValue : 360.0 / vizRepeater.count;
            let rotation = base.startAngle + index * _spacing;
            let idx = index;
            item.x = Qt.binding(()=>{
                                    let len = item.width;
                                    let _spacing = base.spacing === "fixed" ? base.spacingValue : 360.0 / vizRepeater.count;
                                    let _rotation = base.startAngle + idx * _spacing;
                                    let x = Math.cos(_rotation * Math.PI / 180) * (base.radius) + (base.width / 2 - item.width/2);
                                    return x;
                                });
            item.y = Qt.binding(()=>{                                     let len = item.width;
                                    let _spacing = base.spacing === "fixed" ? base.spacingValue : 360.0 / vizRepeater.count;
                                    let _rotation = base.startAngle + idx * _spacing;
                                    return Math.sin(_rotation * Math.PI / 180) * (base.radius) + (base.height / 2 - item.width/2);
                                });
        }

        function calcPosition(_rotation,_rad,_width,_len,_spacing) {
            let xspacing = _spacing === "fixed" ? base.spacingValue : 360.0 / vizRepeater.count;
            return Math.cos(_rotation * Math.PI / 180) * (_rad) + (_width / 2 -_len/2);
        }


    }

    //this repeater lays a MouseArea over the other repeaters elements.
    Repeater {
        id: buttonRepeater
        anchors.fill: parent
        model: base.model
        MouseArea {
            id: mouseArea
            required property int index
            //property var item: buttonRepeater.itemAt(index)
            anchors.fill: parent
            onClicked: {
                console.log("Clicked on item", index);
            }
        }
    }

    function update() {
        for(let i=0; i<vizRepeater.count; i++){
            let item = vizRepeater.itemAt(i);
            let mouse = buttonRepeater.itemAt(i);
            mouse.x = item.x;
            mouse.y = item.y;
            mouse.width = item.width;
            mouse.height = item.height;
        }
    }

    Component.onCompleted: {
        update();
    }

    transform: Translate {
        x: -base.width*0.5
        y: -base.height*0.5
    }

    ClusterView.onMinimumMetChanged: {
        if(ClusterView.minimumMet){
            radialMenu.state = "On"
        }
    }

    ClusterView.onRemoved: {
        radialMenu.state = "Off"
    }

    states: [
        State {
            name: "Start"
            PropertyChanges { target: radialMenu; opacity:0}
            PropertyChanges { target: radialMenu; radius: 0 }

        },
        State {
            name: "Off"
            PropertyChanges { target: radialMenu; opacity:0}
            PropertyChanges { target: radialMenu; radius: 0 }

        },
        State {
            name: "On"
            PropertyChanges { target: radialMenu; opacity:1}
            PropertyChanges { target: radialMenu; radius: 250 }
            PropertyChanges { target: radialMenu; visible: true }
        }

    ]
    transitions: [
        Transition {
            from: "Off,Start"
            to: "On"

            SmoothedAnimation { properties: "opacity,radius"; duration: 250 }
        },
        Transition {
            from: "On"
            to: "Off"

            SequentialAnimation {
                ParallelAnimation {
                    NumberAnimation {
                    target: radialMenu
                    property: "opacity"
                    duration: 200
                    }
                    SmoothedAnimation { properties: "radius"; duration: 250}
                }
                PropertyAction { target: radialMenu; property: "visible"; value:false }
                ScriptAction {
                    script: radialMenu.ClusterView.animateOffFinished()
                }
            }
        }
    ]

}
