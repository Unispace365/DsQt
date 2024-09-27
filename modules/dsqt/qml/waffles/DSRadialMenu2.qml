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
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
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
            //var index = index;
            //var item = item;
            let len = item.width;
            let _spacing = base.spacing === "fixed" ? base.spacingValue : 360.0 / vizRepeater.count;
            let rotation = base.startAngle + index * _spacing;
            item.x = Math.cos(rotation * Math.PI / 180) * (base.radius) + (base.width / 2 -len/2);
            item.y = Math.sin(rotation * Math.PI / 180) * (base.radius) + (base.height / 2 -len/2);
            console.log("Item added", index);
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
            property var item: buttonRepeater.itemAt(index)
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

}
