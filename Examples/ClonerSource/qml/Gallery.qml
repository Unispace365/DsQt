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

    DsSettingsProxy {
        id:appProxy
        target:"app_settings"
        Component.onCompleted: ()=>{
            console.log(appProxy.getSize)
        }
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
        source:DS.env.expand("file:%APP%/data/images/2025_Downstream_Logo_white.svg")
        fillMode: Image.PreserveAspectFit
       //preferredRendererType: VectorImage.CurveRenderer
        Component.onCompleted: {
            console.log("image")
        }

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
                    edge: DsControlSet.Edge.TopOuter
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

                        Button {
                            id: close2
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

        launcher: DsContentLauncher {
            stage: wStage
            model: Ds.getRecordById(Ds.platform?.ambient_playlist)?.children ?? [{'record_name':"woot"},{'record_name':"none"}];
            Component.onCompleted: {
                //console.log("platform_id:"+DS.platform["uid"])
            }
            Connections {
                target: Ds
                function onPlatformChanged() {
                    console.log("playlist:"+DS.platform?.ambient_playlist ?? "none")
                }
            }
        }
    }

    RowLayout {
        anchors.centerIn: root

        DsQuickMenu {
            model: [
                {iconText: "right",
                iconWidth:32,
                iconHeight:32,
                iconPath:"qrc:/res/data/waffles/ui/Cards_256.png"}
                ,{iconText: "left"},
                {}
            ]
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        }

        DsQuickMenu {
            model: [
                {selected: false}
                ,{selected: true}
                ,{selected: false}
                ,{selected: true}
            ]
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        }

        DsQuickMenu {
            model: [
                {selected: false}
                ,{selected: false}
                ,{selected: false}
                ,{selected: true}
                ,{selected: false}
                ,{selected: false}
                ,{selected: false}
                ,{selected: false}
                ,{selected: false}
            ]
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        }

        DsQuickMenu {
            model: [
                {selected: false}
                ,{selected: true}
                ,{selected: false}
                ,{selected: true}
                ,{selected: false}
                ,{selected: false}
                ,{selected: false}
                ,{selected: false}
            ]
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
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
