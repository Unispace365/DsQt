import QtQuick
import QtQuick.Controls
import Dsqt

Window {
    property alias file: log.file

    width: 600
    height: 400
    title: "Text Viewer"
    color: system.base

    //visible: true
    //flags: Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.Tool

    // onScreenChanged: (screen) => {
    //     x = screen.virtualX + 0.5 * (screen.width - width)
    //     y = screen.virtualY + 0.5 * (screen.height - height)
    // }

    // Allows access to the system color palette.
    SystemPalette {
        id: system
    }

    DsTextFileModel {
        id: log
        // file: Ds.env.expand("%LOCAL%/ds_waffles/logs/bridgesync.log")
    }

    Column {
        anchors.fill: parent
        spacing: 10
        anchors.margins: 10

        // Control panel for toggles
        Row {
            spacing: 20
            CheckBox {
                id: wordWrapCheckBox
                checked: false
                text: "Word Wrap"
                palette.accent: system.accent
                palette.light: system.light
                palette.base: system.base
            }

            CheckBox {
                id: autoScrollCheckBox
                checked: true
                text: "Auto Scroll"
                palette.accent: system.accent
                palette.light: system.light
                palette.base: system.base
            }
        }

        // Log display area
        Rectangle {
            width: parent.width
            height: parent.height - wordWrapCheckBox.height - parent.spacing
            color: system.dark
            radius: 5
            clip: true

            ListView {
                id: logView
                anchors.fill: parent
                anchors.margins: 5
                model: log
                clip: true

                delegate: Text {
                    width: logView.width
                    text: model.display
                    wrapMode: wordWrapCheckBox.checked ? Text.Wrap : Text.NoWrap
                    //font.bold: true
                    font.family: "Lucida Sans"
                    font.pointSize: 10
                    lineHeight: 1.2
                    color: system.text
                }

                ScrollBar.vertical: ScrollBar {
                    active: true
                }

                // Handle auto-scroll when content updates
                Connections {
                    target: log
                    function onContentUpdated() {
                        if (autoScrollCheckBox.checked) {
                            logView.positionViewAtEnd()
                        }
                    }
                }

                Component.onCompleted: logView.positionViewAtEnd()
            }
        }
    }
}
