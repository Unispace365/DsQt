import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage
import Dsqt.Core
import Dsqt.Waffles

// Per-type fullscreen controller for media viewers. The stage shows this while a viewer is
// fullscreen and binds it to that viewer. It is moveable (drag the title or the collapse tab)
// and collapsible (collapsed = just the tab). Layout: a left collapse tab + a bar with the
// title on one line and the per-type media controls (DsMediaControls) below, plus the window
// buttons (fullscreen-exit, close).
Item {
    id: controller

    // The fullscreen viewer this controller drives (set by the stage).
    property var viewer: null
    // Set by the stage to fade the bar in/out.
    property bool shown: false
    // Collapsed = only the tab is shown.
    property bool collapsed: false

    readonly property var model: viewer ? viewer.model : null

    // The fullscreen controller does NOT auto-hide (it is moveable + collapsible instead); it just
    // fades in/out with `shown`.
    width: shell.implicitWidth
    height: shell.implicitHeight
    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

    // Centre near the bottom the first time it is shown; draggable thereafter.
    property bool _placed: false
    onShownChanged: {
        if (shown && !_placed && parent) {
            x = Math.round((parent.width - width) / 2);
            y = Math.round(parent.height - height - 120);
            _placed = true;
        }
    }

    function windowIcon(n) { return Ds.env.expandUrl("file:///%APP%/data/images/waffles/window/" + n) }

    // Centred, colourised vector icon with a click area.
    component IconBtn: Item {
        id: ib
        property url icon
        signal clicked()
        implicitWidth: 48
        implicitHeight: 48
        VectorImage {
            anchors.centerIn: parent
            width: 30
            height: 30
            source: ib.icon
            preferredRendererType: VectorImage.CurveRenderer
            layer.enabled: true
            layer.effect: MultiEffect { colorization: 1.0; colorizationColor: DsTheme.surfaceText }
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: ib.clicked() }
    }

    RowLayout {
        id: shell
        spacing: 12

        // Collapse tab — the only thing left when collapsed; also a drag grip.
        Item {
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            Layout.alignment: Qt.AlignVCenter

            DsGlassBackground {
                anchors.fill: parent
                fallbackColor: DsTheme.scrim
                borderColor: DsTheme.stroke
                borderWidth: DsTheme.glassBorderWidth
                topLeftRadius: DsTheme.glassRadius
                topRightRadius: DsTheme.glassRadius
                bottomLeftRadius: DsTheme.glassRadius
                bottomRightRadius: DsTheme.glassRadius
            }
            IconBtn {
                anchors.fill: parent
                icon: controller.collapsed ? controller.windowIcon("controller_expand.svg")
                                           : controller.windowIcon("controller_collapse.svg")
                onClicked: controller.collapsed = !controller.collapsed
            }
            DragHandler {
                target: controller
                xAxis.minimum: 0
                xAxis.maximum: controller.parent ? controller.parent.width - controller.width : 0
                yAxis.minimum: 0
                yAxis.maximum: controller.parent ? controller.parent.height - controller.height : 0
            }
        }

        // Main bar — title on top, media controls below.
        Item {
            id: bar
            visible: !controller.collapsed
            Layout.preferredWidth: 1400
            Layout.preferredHeight: 160
            Layout.alignment: Qt.AlignVCenter

            DsGlassBackground {
                anchors.fill: parent
                fallbackColor: DsTheme.scrim
                borderColor: DsTheme.stroke
                borderWidth: DsTheme.glassBorderWidth
                topLeftRadius: DsTheme.glassRadius
                topRightRadius: DsTheme.glassRadius
                bottomLeftRadius: DsTheme.glassRadius
                bottomRightRadius: DsTheme.glassRadius
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                // Title row (drag grip).
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: controller.model && controller.model.title ? controller.model.title : "Media"
                        color: DsTheme.surfaceText
                        font.family: "Roboto"
                        font.pixelSize: 30
                        elide: Text.ElideRight
                    }
                    DragHandler {
                        target: controller
                        xAxis.minimum: 0
                        xAxis.maximum: controller.parent ? controller.parent.width - controller.width : 0
                        yAxis.minimum: 0
                        yAxis.maximum: controller.parent ? controller.parent.height - controller.height : 0
                    }
                }

                // Media-controls row: the per-type controls (transport / page / nav) + window buttons.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 14

                    DsMediaControls {
                        id: fsMedia
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        chrome: false
                        // DsControlSet defaults to opacity 0; force visible here (the bar itself
                        // does not auto-hide).
                        opacity: 1
                        signalObject: controller.viewer
                    }
                    // Pushes the window buttons right when there are no media controls (e.g. image).
                    Item { Layout.fillWidth: true; visible: !fsMedia.visible }

                    IconBtn {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        Layout.alignment: Qt.AlignVCenter
                        icon: controller.windowIcon("fullscreen.svg")
                        onClicked: if (controller.viewer) controller.viewer.toggleFullscreen()
                    }
                    IconBtn {
                        Layout.preferredWidth: 48
                        Layout.preferredHeight: 48
                        Layout.alignment: Qt.AlignVCenter
                        icon: controller.windowIcon("close.svg")
                        onClicked: if (controller.viewer) controller.viewer.closeRequested()
                    }
                }
            }
        }
    }
}
