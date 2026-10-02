pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import Dsqt.Waffles

DsControlSet {
    id: toolbar
    edge: DsControlSet.BottomInner
    offset: DsTheme.dp(69)
    implicitWidth: DsTheme.dp(1141)
    implicitHeight: 102 * panel.scale
    readonly property DsWhiteboard board: signalObject as DsWhiteboard
    property var strokeSizes: [6, 18, 36]
    property var colors: ["#000000", "#ffffff", "#8f51f3", "#0fffa7",
                          "#0fbfff", "#ff3b0f", "#ff7b0f", "#ffd30f"]
    readonly property string iconRoot: "qrc:/qt/qml/Dsqt/Waffles/data/images/waffles/whiteboard/"

    component ToolButton: Button {
        id: button
        property string glyph: ""
        property bool selected: false
        property real cornerRadius: 16
        property bool flatBackground: false
        width: 58
        height: 58
        padding: 0
        Accessible.name: text
        ToolTip.visible: hovered
        ToolTip.text: text
        onClicked: toolbar.interacted()
        background: Rectangle {
            radius: button.cornerRadius
            color: button.selected || button.down ? DsTheme.accent
                 : button.flatBackground ? "transparent" : DsTheme.surfaceVariant
            border.width: button.activeFocus ? 2 : 0
            border.color: DsTheme.surfaceText
        }
        contentItem: Item {
            Image {
                anchors.centerIn: parent
                width: button.glyph === "close" ? 20 : 40
                height: width
                sourceSize: Qt.size(width, height)
                source: button.glyph ? toolbar.iconRoot + button.glyph + ".svg" : ""
                opacity: button.enabled ? 1 : 0.35
            }
        }
    }

    Item {
        id: panel
        width: 1141
        height: 102
        scale: Math.min(DsTheme.uiScale, toolbar.width / width)
        transformOrigin: Item.TopLeft

        DsGlassBackground {
            width: 1126
            height: 102
            source: toolbar.board?.captureItem ?? null
            viewerItem: toolbar.board
            refresh: toolbar.x + toolbar.y + panel.scale
            topLeftRadius: 20
            topRightRadius: 20
            bottomLeftRadius: 20
            bottomRightRadius: 20
            tint: DsTheme.surface
            tintOpacity: DsTheme.glassTintOpacity
            fallbackColor: DsTheme.scrim
            borderColor: DsTheme.stroke
            borderWidth: 1
        }
        // Consume gaps in the toolbar so a press between buttons does not draw.
        MouseArea { width: 1126; height: 102; acceptedButtons: Qt.AllButtons }
        Row {
            x: 22; y: 23; spacing: 22
            ToolButton {
                objectName: "penButton"
                text: qsTr("Pen"); glyph: "pen"
                selected: toolbar.board?.tool === DsWhiteboardTool.Pen
                onClicked: if (toolbar.board) toolbar.board.tool = DsWhiteboardTool.Pen
            }
            ToolButton {
                objectName: "highlighterButton"
                text: qsTr("Highlighter"); glyph: "highlighter"
                selected: toolbar.board?.tool === DsWhiteboardTool.Highlighter
                onClicked: if (toolbar.board) toolbar.board.tool = DsWhiteboardTool.Highlighter
            }
            ToolButton {
                objectName: "eraserButton"
                text: qsTr("Eraser"); glyph: "eraser"
                selected: toolbar.board?.tool === DsWhiteboardTool.Eraser
                onClicked: if (toolbar.board) toolbar.board.tool = DsWhiteboardTool.Eraser
            }
        }
        Row {
            x: 271; y: 31; spacing: 10
            Repeater {
                model: toolbar.strokeSizes
                ToolButton {
                    id: sizeButton
                    required property int index
                    required property real modelData
                    width: 40; height: 40; cornerRadius: 10
                    text: qsTr("Stroke size %1").arg(modelData)
                    selected: toolbar.board?.strokeWidth === modelData
                    onClicked: if (toolbar.board) toolbar.board.strokeWidth = modelData
                    contentItem: Item {
                        Rectangle {
                            anchors.centerIn: parent
                            width: [11, 17, 27][Math.min(sizeButton.index, 2)]
                            height: width
                            radius: width / 2
                            color: DsTheme.surfaceText
                        }
                    }
                }
            }
        }
        Row {
            x: 425; y: 31; spacing: -2
            Repeater {
                model: toolbar.colors
                ToolButton {
                    id: colorButton
                    required property color modelData
                    width: 39; height: 39
                    text: qsTr("Color %1").arg(modelData)
                    selected: toolbar.board?.strokeColor === modelData
                    onClicked: if (toolbar.board) toolbar.board.strokeColor = modelData
                    background: Rectangle {
                        radius: width / 2
                        color: "transparent"
                        border.width: colorButton.selected || colorButton.activeFocus ? 2 : 0
                        border.color: DsTheme.accent
                    }
                    contentItem: Item {
                        Rectangle {
                            anchors.centerIn: parent
                            width: 27; height: 27; radius: 13.5
                            color: colorButton.modelData
                        }
                    }
                }
            }
        }
        Rectangle { x: 739; y: 18; width: 2; height: 64; color: DsTheme.stroke }
        ToolButton {
            objectName: "backgroundButton"
            x: 761; y: 20
            text: qsTr("White background"); glyph: "background"
            selected: toolbar.board?.solidBackground ?? false
            onClicked: if (toolbar.board) toolbar.board.solidBackground = !toolbar.board.solidBackground
        }
        Rectangle { x: 841; y: 19; width: 2; height: 64; color: DsTheme.stroke }
        ToolButton {
            objectName: "undoButton"
            x: 863; y: 30; width: 40; height: 40; flatBackground: true
            text: qsTr("Undo"); glyph: "undo"
            enabled: toolbar.board?.canUndo ?? false
            onClicked: toolbar.board.undo()
        }
        ToolButton {
            objectName: "redoButton"
            x: 925; y: 30; width: 40; height: 40; flatBackground: true
            text: qsTr("Redo"); glyph: "redo"
            enabled: toolbar.board?.canRedo ?? false
            onClicked: toolbar.board.redo()
        }
        ToolButton {
            objectName: "saveButton"
            x: 1009; y: 19
            text: qsTr("Save"); glyph: "save"
            enabled: toolbar.board !== null && !toolbar.board.saving && !toolbar.board.drawing
            onClicked: toolbar.board.requestSave()
        }
        ToolButton {
            objectName: "closeButton"
            x: 1111; y: 35; width: 30; height: 30; cornerRadius: 8
            text: qsTr("Close"); glyph: "close"
            onClicked: if (toolbar.board) toolbar.board.close()
        }
    }
}
