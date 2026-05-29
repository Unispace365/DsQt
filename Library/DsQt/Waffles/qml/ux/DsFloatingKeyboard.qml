pragma ComponentBehavior: Bound

import QtQuick
import Dsqt.Waffles

// Floating, draggable virtual keyboard for text entry over viewers that aren't the launcher
// (primarily the web viewer). Wraps DsVirtualKeyboard in a dark rounded frame with a drag bar and
// a close button. Toggled via DsWaffleStage.floatingKeyboardShown and hosted on the stage's
// modal3 (top) layer so it floats above everything and isn't clipped.
//
// Width is comfortable for a 4K stage (the launcher's docked keyboard is narrow by necessity);
// the InputPanel derives its height from that width. The frame is shown as soon as it's toggled
// on so the user can then tap a field; keys route once a control (e.g. a web input) takes focus.
//
// NOTE: shares the single global InputContext with the launcher's docked keyboard, so only one
// should be active at a time — the launcher gates its docked panel off while this is shown.
Item {
    id: fkb

    // Comfortable keyboard width for a 4K stage.
    property int kbWidth: 1400
    signal closeRequested()

    width: kbWidth
    height: frameCol.implicitHeight
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    // Initial placement comes from the host Loader's anchors (bottom-centre of the stage); the
    // drag bar then repositions from there. Sizing to the keyboard's implicit size means the host
    // Loader must NOT be given an explicit size, else it would resize this item.

    // Dark rounded frame. (A blurred glass frame would need the cross-layer slot source plumbed
    // in like the viewers/launcher; the keyboard style already paints its own dark surface, so a
    // flat tinted frame reads fine here. Upgradeable later — see backlog.)
    Rectangle {
        anchors.fill: parent
        radius: 20
        color: Qt.rgba(DsTheme.surface.r, DsTheme.surface.g, DsTheme.surface.b, 0.92)
        border.color: DsTheme.stroke
        border.width: DsTheme.glassBorderWidth
    }

    Column {
        id: frameCol
        width: parent.width

        // Drag bar + close button.
        Item {
            width: parent.width
            height: 44

            DragHandler { target: fkb }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: "Keyboard"
                color: DsTheme.surfaceText
                font.family: "Roboto"
                font.weight: 100
                font.pixelSize: 16
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 28
                height: 28
                radius: 8
                color: DsTheme.surfaceVariant
                Text {
                    anchors.centerIn: parent
                    text: "✕"
                    color: DsTheme.surfaceText
                    font.pixelSize: 14
                }
                TapHandler { onTapped: fkb.closeRequested() }
            }
        }

        DsVirtualKeyboard {
            id: kbd
            width: parent.width
        }
    }
}
