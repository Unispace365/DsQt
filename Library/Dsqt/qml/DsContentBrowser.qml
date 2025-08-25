pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Dsqt

Window {
    width: 1500
    height: 500
    title: "Content Model Viewer"
    visible: true

    //parent: root
    DsContentModelItemModel {
        id: myModel
    }

    ColumnLayout {
        anchors.fill: parent
        SplitView {
            id: sview
            Layout.fillHeight: true
            Layout.fillWidth: true
            orientation: Qt.Horizontal

            TreeView {
                id: data
                model: myModel
                SplitView.maximumHeight: sview.height
                SplitView.minimumWidth: sview.width / 3
                SplitView.maximumWidth: sview.width / 3 * 2

                delegate: TreeViewDelegate {
                    font.pixelSize: 25
                    text: model.display
                    onClicked: {
                        console.log("Clicked:", model.display)
                        let array = []
                        Object.keys(model.contentModel)?.forEach(
                                    function (key) {
                                        let obj = {
                                            "key": key,
                                            "value": model.contentModel[key]
                                        }
                                        array.push(obj)
                                    })
                        listView.model = array
                        //console.log(JSON.stringify(model.contentModel, null, 2))
                    }
                }
            }

            Item {
                id: container
                SplitView.maximumHeight: sview.height
                SplitView.minimumWidth: sview.width / 3
                SplitView.maximumWidth: sview.width / 3 * 2
                SplitView.fillWidth: true
                Rectangle {
                    anchors.fill: parent
                    color: "#ffffff"
                }

                ListView {
                    id: listView
                    anchors.fill: parent

                    delegate: Item {
                        id: itemDelegate
                        height: childrenRect.height
                        width: listView.width
                        required property var modelData
                        property bool isObj: modelData.value.filepath !== undefined
                        property bool showMediaProps: false

                        RowLayout {
                            id: rlayout
                            height: childrenRect.height
                            width: listView.width

                            Text {
                                font.pixelSize: 25
                                //color: "#c2f4c6"
                                text: itemDelegate.modelData.key + ":"
                                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                            }
                            Item {
                                implicitHeight: childrenRect.height
                                Layout.fillWidth: true
                                Button {
                                    id: buton
                                    height: model_value.height
                                    text: itemDelegate.showMediaProps ? "less" : "more"
                                    font.pixelSize: 18
                                    visible: itemDelegate.isObj
                                    onClicked: {
                                        itemDelegate.showMediaProps = !itemDelegate.showMediaProps
                                        //console.log("Show media props:", itemDelegate.showMediaProps);
                                    }
                                }
                                Text {
                                    id: model_value
                                    anchors.left: buton.right
                                    Layout.alignment: Qt.AlignLeft | Qt.AlignTop

                                    font.pixelSize: 25
                                    text: itemDelegate.isObj ? itemDelegate.modelData.value.filepath : itemDelegate.modelData.value
                                }
                            }
                        }
                        Item {
                            anchors.top: rlayout.bottom
                            width: itemDelegate.showMediaProps ? childrenRect.width : 0
                            height: itemDelegate.showMediaProps ? childrenRect.height : 0
                            visible: itemDelegate.showMediaProps
                            SplitView {
                                width: container.width
                                height: extraInfo.height
                                orientation: Qt.Horizontal
                                Flickable {
                                    height: extraInfo.height
                                    SplitView.minimumWidth: container.width * .25
                                    SplitView.maximumWidth: container.width
                                    SplitView.fillWidth: true
                                    contentWidth: extraInfo.width
                                    contentHeight: extraInfo.height
                                    clip: true
                                    Text {

                                        id: extraInfo
                                        color: "#808080"
                                        text: JSON.stringify(itemDelegate.modelData.value,
                                                             null, 2)
                                    }
                                }
                                Image {
                                    id: image
                                    SplitView.maximumHeight: extraInfo.height
                                    SplitView.minimumWidth: container.width * 0.25
                                    SplitView.maximumWidth: container.width
                                    SplitView.fillWidth: true
                                    visible: itemDelegate.isObj
                                             && itemDelegate.modelData.value.filepath !== undefined
                                    source: itemDelegate.modelData.value.filepath
                                    fillMode: Image.PreserveAspectFit
                                }
                            }
                        }
                    }

                    model: null
                }
            }

            handle: Rectangle {
                id: handleDelegate
                implicitWidth: 16
                implicitHeight: 16
                color: SplitHandle.pressed ? "#81e889" : (SplitHandle.hovered ? Qt.lighter("#c2f4c6", 1.1) : "#c2f4c6")
            }
        }
        Button {
            text: "Reload" + (myModel?.isDirty ? " (dirty)" : "")
            Layout.fillWidth: true
            onClicked: {
                myModel.reload()
                console.log("Reloaded model")
            }
        }
    }
}
