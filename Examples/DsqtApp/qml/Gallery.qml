pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.VectorImage

Item {
    id:root
    property real fps:0
    property real lastUpdateTime:0

    Rectangle {
        anchors.fill: parent
        color: "Yellow"
    }

    /*Downstream {
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

    }*/

    VectorImage {
        id:vds
        source:"../data/images/2025_Downstream_Logo_white.svg"
        anchors.fill: root
        fillMode: VectorImage.PreserveAspectFit
        preferredRendererType: VectorImage.CurveRenderer

    }

    Text {
        width: 200;
        height: 100;
        color: "white"
        text: "FPS:"+root.fps.toFixed(2)
        font.pixelSize: 50
    }

    FrameAnimation {
        running: true
        onTriggered: {
            if(elapsedTime - root.lastUpdateTime>0.25){
                root.fps = 1/frameTime;
                root.lastUpdateTime = elapsedTime;
            }
        }
    }
}
