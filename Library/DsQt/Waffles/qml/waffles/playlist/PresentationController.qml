pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.VectorImage
import Dsqt.Core
import Dsqt.Waffles

// Movable, collapsible on-screen controls for a playing playlist (DsPlaylistViewer) — mirrors the
// FullscreenController's tab+bar layout, but drives a playlist rather than a fullscreen media
// viewer (a playlist is not media and has no fullscreen state). Bar: prev · play/pause · "i / N" ·
// next · close.
//
// Origin (0,0) = the collapse-tab centre; the bar's LEFT EDGE sits at the origin so the tab
// straddles the bar's left edge and stays put when collapsing. Positioned by x/y (set on first
// show), draggable thereafter, clamped to the parent. Panels are a solid frosted tint (a playlist
// is full-stage opaque content, so there's no cross-layer glass to sample like the FS controller).
Item {
    id: pc

    property var  playlist: null     // the DsPlaylistViewer this controls (set by the stage)
    property bool shown: false
    property bool collapsed: false
    signal closeRequested()

    // Sizes in chrome units (DsTheme.dp) so it scales with uiScale.
    property int tabSize:   DsTheme.dp(30)
    property int barWidth:  DsTheme.dp(300)
    property int barHeight: DsTheme.dp(56)
    property int barRadius: DsTheme.dp(20)
    property int tabRadius: DsTheme.dp(8)

    readonly property int  _index:  pc.playlist ? pc.playlist.index : 0
    readonly property int  _count:  pc.playlist ? pc.playlist.count : 0
    readonly property bool _paused: pc.playlist ? (pc.playlist.paused === true) : false

    // Content extents from the origin (tab centre / bar left edge), for drag clamping.
    readonly property real _left:  tabSize / 2
    readonly property real _right: collapsed ? tabSize / 2 : barWidth
    readonly property real _vert:  collapsed ? tabSize / 2 : barHeight / 2

    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

    // Place centred near the bottom on first show (origin is the tab centre, so offset left by half
    // the bar). Draggable thereafter.
    property bool _placed: false
    onShownChanged: {
        if (shown && !_placed && parent) {
            x = Math.round(parent.width / 2 - barWidth / 2);
            y = Math.round(parent.height - barHeight / 2 - DsTheme.dp(120));
            _placed = true;
        }
    }

    function media(n) { return "qrc:/qt/qml/Dsqt/Waffles/data/images/waffles/media/" + n }
    function win(n)   { return Ds.env.expandUrl("file:///%APP%/data/images/waffles/window/" + n) }

    component IconBtn: Item {
        id: ib
        property url icon
        property bool active: false
        signal clicked()
        implicitWidth: DsTheme.dp(32)
        implicitHeight: DsTheme.dp(32)
        VectorImage {
            anchors.centerIn: parent
            width: DsTheme.dp(20); height: DsTheme.dp(20)
            source: ib.icon
            preferredRendererType: VectorImage.CurveRenderer
            layer.enabled: true
            layer.effect: MultiEffect { colorization: 1.0; colorizationColor: ib.active ? DsTheme.accent : DsTheme.surfaceText }
        }
        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: ib.clicked() }
    }

    // Collapse tab — centred on the origin; never moves when toggling.
    Item {
        id: tab
        width: pc.tabSize; height: pc.tabSize
        x: -width / 2; y: -height / 2
        z: 1
        Rectangle {
            anchors.fill: parent
            radius: pc.tabRadius
            color: Qt.rgba(DsTheme.surface.r, DsTheme.surface.g, DsTheme.surface.b, 0.92)
            border.color: DsTheme.stroke
            border.width: DsTheme.glassBorderWidth
        }
        IconBtn {
            anchors.fill: parent
            icon: pc.collapsed ? pc.win("controller_expand.svg") : pc.win("controller_collapse.svg")
            onClicked: pc.collapsed = !pc.collapsed
        }
        DragHandler {
            target: pc
            xAxis.minimum: pc._left
            xAxis.maximum: pc.parent ? pc.parent.width - pc._right : 0
            yAxis.minimum: pc._vert
            yAxis.maximum: pc.parent ? pc.parent.height - pc._vert : 0
        }
    }

    // Bar — left edge at the origin, vertically centred on it.
    Item {
        id: bar
        visible: !pc.collapsed
        width: pc.barWidth; height: pc.barHeight
        x: 0; y: -height / 2
        Rectangle {
            anchors.fill: parent
            radius: pc.barRadius
            color: Qt.rgba(DsTheme.surface.r, DsTheme.surface.g, DsTheme.surface.b, 0.92)
            border.color: DsTheme.stroke
            border.width: DsTheme.glassBorderWidth
        }
        // Drag the controller by any non-button part of the bar.
        DragHandler {
            target: pc
            xAxis.minimum: pc._left
            xAxis.maximum: pc.parent ? pc.parent.width - pc._right : 0
            yAxis.minimum: pc._vert
            yAxis.maximum: pc.parent ? pc.parent.height - pc._vert : 0
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: DsTheme.dp(24)   // clear the collapse tab overlapping the left edge
            anchors.rightMargin: DsTheme.dp(12)
            spacing: DsTheme.dp(8)
            IconBtn {
                Layout.alignment: Qt.AlignVCenter
                icon: pc.media("arrow_left.svg")
                onClicked: if (pc.playlist) pc.playlist.prev()
            }
            IconBtn {
                Layout.alignment: Qt.AlignVCenter
                icon: pc._paused ? pc.media("play.svg") : pc.media("pause.svg")
                onClicked: if (pc.playlist) pc.playlist.paused = !pc.playlist.paused
            }
            Text {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: (pc._count > 0) ? ((pc._index + 1) + " / " + pc._count) : "—"
                color: DsTheme.surfaceText
                font.family: "Roboto"; font.pixelSize: DsTheme.dp(14)
                horizontalAlignment: Text.AlignHCenter
            }
            IconBtn {
                Layout.alignment: Qt.AlignVCenter
                icon: pc.media("arrow_right.svg")
                onClicked: if (pc.playlist) pc.playlist.next()
            }
            IconBtn {
                Layout.alignment: Qt.AlignVCenter
                icon: pc.win("close.svg")
                onClicked: pc.closeRequested()
            }
        }
    }
}
