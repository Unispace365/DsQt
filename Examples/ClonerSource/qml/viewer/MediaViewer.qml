import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage
import Dsqt

DsTitledMediaViewer {
            id: viewer

            //source:DS.env.expand("file:%APP%/data/images/landscape.jpeg")


            controls: [
                DsControlSet {
                    id:controlRoot
                    edge: DsControlSet.Edge.TopOuter
                    height: titleText.height
                    //required property string contentModel
                    Item {
                        Rectangle {
                            height: titleText.height + 8*2
                            width: parent.width
                            topLeftRadius: 20
                            topRightRadius: 20
                            color: palette.button
                        }

                        Text {
                            id:titleText
                            text: viewer.model.title ?? "Media"
                        }
                    }
                },
                DsControlSet {
                    id: rightButtons
                    edge: DsControlSet.Edge.RightOuter
                    width: 32


                    ColumnLayout {
                        anchors.fill: rightButtons
                        spacing: 0
                        Item {
                            Layout.preferredWidth: 37
                            Layout.preferredHeight: 37
                            Layout.alignment: Qt.AlignRight | Qt.AlignBottom


                            Button {
                                id: close
                                height: 37
                                width: 37
                                padding: 0

                                background: Rectangle {
                                    color: palette.button
                                    border.width: 1
                                    border.color: palette.light
                                    anchors.fill: parent
                                    topRightRadius: 20
                                    bottomRightRadius: 20
                                }
                                Item {
                                    x:6
                                    y:10
                                    width:16
                                    height:16
                                    Rectangle {
                                        anchors.fill: parent
                                        color: "red"
                                        visible:false
                                    }

                                    VectorImage {
                                        id:icon
                                        anchors.fill: parent
                                        source: Ds.env.expandUrl("file:///%APP%/data/images/waffles/window/close.svg")
                                        preferredRendererType: VectorImage.CurveRenderer
                                        layer.enabled: false
                                        layer.live: true
                                        layer.effect: MultiEffect  {
                                            colorization: 1.0
                                            colorizationColor: "red"
                                        }
                                    }

                                }
                                onPressed: {
                                    viewer.destroy();
                                }

                                DragHandler {
                                    target:viewer
                                }
                            }
                        }
                    }
                }

            ]
        }


