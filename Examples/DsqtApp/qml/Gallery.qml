import QtQuick
import QtQuick.VectorImage

Item {
    id:base
    property real fps:0
    property real lastUpdateTime:0

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Downstream {
        id:ds
        width: implicitWidth
        height: implicitHeight
        property real pw : parent!=null?parent.width:width
        property real ph : parent!=null?parent.height:height
        property real margin: 100
            transform: [
                Scale {
                    id: scale; xScale: yScale;
                    yScale: Math.min(
                        (ds.pw-ds.margin)/ds.width,
                        (ds.ph-ds.margin)/ds.height);},
                Translate {
                    x: (ds.pw-ds.width*scale.xScale)/2;
                    y: (ds.ph-ds.height*scale.yScale)/2;}
            ]

    }

    Text {
        width: 200;
        height: 100;
        color: "white"
        text: "FPS:"+base.fps.toFixed(2)
        font.pixelSize: 50
    }

    FrameAnimation {
        running: true
        onTriggered: {
            if(elapsedTime - base.lastUpdateTime>0.25){
                base.fps = 1/frameTime;
                base.lastUpdateTime = elapsedTime;
            }
        }
    }

}
