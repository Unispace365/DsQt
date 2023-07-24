import QtQuick
import QtMultimedia
import dsqml

Item {
    id:root
    property var x_model;
    property var color : "red";
    property alias model: tree.model;
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - parent.width* root.x_model;
        height: parent.height - parent.height* root.x_model;
        color: root.color
    }

    Image {
        id: image
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
    }

    MediaPlayer {
        id: video
        videoOutput: mediaImgOut
        onSourceChanged: video.play()
        loops: MediaPlayer.Infinite
    }

    VideoOutput {
        id: mediaImgOut
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
    }

    Rectangle {
        id: backing
        anchors.fill: tree
        color: Qt.rgba(1,1,1,0.5);
    }

    TreeView {
        id: tree
        height: parent.height
        width: childrenRect.width
        delegate: Item {
            id: treeDelegate

            implicitWidth: padding + label.x + label.implicitWidth + padding
            implicitHeight: label.implicitHeight * 1.5

            readonly property real indent: 20
            readonly property real padding: 5

            // Assigned to by TreeView:
            required property TreeView treeView
            required property bool isTreeNode
            required property bool expanded
            required property int hasChildren
            required property int depth

            TapHandler {
                onTapped: {
                    treeView.toggleExpanded(row);
                    var res = model.display.media_res
                    if(res){
                        if(res.resourceType === DSResource.VIDEO){
                            video.source = model.display.media_res.url;
                            image.source="";
                        } else if(res.resourceType === DSResource.IMAGE){
                            image.source = model.display.media_res.url;
                            video.source="";
                        }
                    }
                }
            }



            Text {
                id: indicator
                visible: treeDelegate.isTreeNode && treeDelegate.hasChildren
                x: padding + (treeDelegate.depth * treeDelegate.indent)
                anchors.verticalCenter: label.verticalCenter
                text: "▸"
                rotation: treeDelegate.expanded ? 90 : 0

            }

            Text {
                id: label
                x: padding + (treeDelegate.isTreeNode ? (treeDelegate.depth + 1) * treeDelegate.indent : 0)
                width: treeDelegate.width - treeDelegate.padding - x
                clip: true
                text: model.display.name === undefined ? "Content Tree" : model.display.name
            }


        }

    }
}
