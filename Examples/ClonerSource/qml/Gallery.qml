pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.VectorImage
import QtQuick.Controls
import QtQuick.Layouts
import Dsqt

Item {
    id:root
    property real fps:0
    property real lastUpdateTime:0
    anchors.fill: parent
    Rectangle {
        anchors.fill: parent
        color: "black"
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

    Image {
        id:vds
        anchors.right: root.right
        anchors.bottom: root.bottom
        source:Ds.env.expand("file:%APP%/data/images/2025_Downstream_Logo_white.svg")
        fillMode: Image.PreserveAspectFit
       //preferredRendererType: VectorImage.CurveRenderer

    }

    Rectangle {
        x: vds.x
        y: vds.y
        width: vds.width
        height: vds.height
        color: "transparent"
        border.width: 1
        border.color: "black"
    }

    DsWaffleStage {
        id: wStage
        anchors.fill: root
        viewer: DsTitledMediaViewer {
            id: viewer
            source:DS.env.expand("file:%APP%/data/images/landscape.jpeg")
            controls: [
                DsControlSet {
                    id:controlRoot
                    edge: DsWaffleStage.Edge.TopOuter
                    height: close.height
                    //required property string contentModel

                    RowLayout {
                        anchors.fill: controlRoot
                        Text {
                            text: "Title"
                        }
                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            id: close
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            Layout.alignment: Qt.AlignRight
                            icon.source: "qrc:///res/data/waffles/icons/1x/Close_64.png"
                            icon.width: 32
                            icon.height: 32
                            icon.color: "black"
                            onPressed: {
                                viewer.destroy();
                            }

                            DragHandler {
                                target:viewer
                            }
                        }
                    }
                }

            ]
        }

        launcher: DsTestLauncher {
            stage: wStage
        }
    }

    Text {
        width: 250;
        height: 100;
        anchors.right: root.right
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
