import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage
import Dsqt.Core
import Dsqt.Waffles

// The media-viewer fullscreen controller. Shown by the stage while a media viewer is fullscreen
// (and bound to it). Moveable (drag the bar or the collapse tab) and collapsible (collapsed = just
// the tab). Layout: a left collapse tab + a bar with the title on one line and the per-type media
// controls (DsMediaControls) below, plus the window buttons.
//
// Extends DsController for the controller contract (viewer / shown / collapsed) and the frosted
// `glassContext` — the tab & bar panels are real glass over the fullscreen media.
//
// The controller's origin (0,0) is the CENTRE of the collapse tab. The tab is centred on it and
// the bar's LEFT EDGE sits at the origin too — so the tab's middle straddles the bar's left edge
// (the tab overlaps it, drawn on top). The bar extends right, vertically centred on the origin, so
// the tab stays put when the bar appears/disappears (collapse).
DsController {
    id: controller

    property int tabSize: 56
    property int barWidth: 1400
    property int barHeight: 160

    // Content extents from the origin (= tab centre / bar's left edge), for placement & clamping.
    readonly property real _left:  tabSize / 2
    readonly property real _right: collapsed ? tabSize / 2 : barWidth
    readonly property real _vert:  collapsed ? tabSize / 2 : barHeight / 2

    // On first show, place so the bar sits centred near the bottom (origin is the tab centre, so
    // offset left by half the bar). Draggable thereafter.
    property bool _placed: false
    onShownChanged: {
        if (shown && !_placed && parent) {
            x = Math.round(parent.width / 2 - barWidth / 2);
            y = Math.round(parent.height - barHeight / 2 - 120);
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

    // Collapse tab — centred on the controller origin (0,0); never moves when toggling.
    Item {
        id: tab
        width: controller.tabSize
        height: controller.tabSize
        x: -width / 2
        y: -height / 2
        z: 1   // on top of the bar where they overlap

        DsGlassBackground {
            anchors.fill: parent
            context: controller.glassContext
            fallbackColor: DsTheme.scrim
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
            xAxis.minimum: controller._left
            xAxis.maximum: controller.parent ? controller.parent.width - controller._right : 0
            yAxis.minimum: controller._vert
            yAxis.maximum: controller.parent ? controller.parent.height - controller._vert : 0
        }
    }

    // Main bar — left edge at the origin (= the tab's centre), vertically centred on it.
    Item {
        id: bar
        visible: !controller.collapsed
        width: controller.barWidth
        height: controller.barHeight
        x: 0
        y: -height / 2

        DsGlassBackground {
            anchors.fill: parent
            context: controller.glassContext
            fallbackColor: DsTheme.scrim
            topLeftRadius: DsTheme.glassRadius
            topRightRadius: DsTheme.glassRadius
            bottomLeftRadius: DsTheme.glassRadius
            bottomRightRadius: DsTheme.glassRadius
        }

        // Drag the controller by any non-interactive part of the bar — the title, the gaps, or the
        // whole controls area when the media type has no controls (e.g. an image). The actual
        // controls (sliders/buttons) sit on top and take their own input.
        DragHandler {
            target: controller
            xAxis.minimum: controller._left
            xAxis.maximum: controller.parent ? controller.parent.width - controller._right : 0
            yAxis.minimum: controller._vert
            yAxis.maximum: controller.parent ? controller.parent.height - controller._vert : 0
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            anchors.leftMargin: 48   // clear the collapse tab overlapping the bar's left edge
            spacing: 10

            // Title row.
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
                    // DsControlSet defaults to opacity 0; force visible here (the bar doesn't auto-hide).
                    opacity: 1
                    signalObject: controller.viewer
                }
                // Empty, draggable space when there are no media controls (e.g. image); also pushes
                // the window buttons to the right.
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
