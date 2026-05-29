pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtMultimedia
import Dsqt.Core
import Dsqt.Waffles

// Per-media-type controls (video/audio, pdf, web; image has none). Reused in two places:
//   - docked at the viewer's bottom-inner edge (windowed), drawing its own glass pill (chrome)
//   - inside the fullscreen controller bar (chrome off)
// Excludes fullscreen/close (those are the viewer's corner buttons / the fullscreen controller).
DsControlSet {
    id: mc
    edge: DsControlSet.Edge.BottomInner
    // Draw the standalone glass pill (windowed). Turned off when embedded in the fullscreen bar.
    property bool chrome: true

    readonly property var    viewer: signalObject
    readonly property var    mv:     viewer ? viewer.mediaViewer : null
    readonly property string mtype:  mv ? mv.mediaType : ""
    readonly property var    item:   mv ? mv.mediaItem : null
    readonly property bool   hasControls: mtype === "video" || mtype === "pdf" || mtype === "web"

    // Sized to Figma's pill spec (37 px tall). Parent Layouts (the FS controller's RowLayout)
    // can override with Layout.fillHeight; nothing else hardcodes the height.
    implicitHeight: DsTheme.dp(37)
    // Visible only when the media type has controls and not faded out (so it doesn't take input
    // while hidden). In the fullscreen bar its opacity is forced to 1 and the whole bar fades.
    visible: hasControls && opacity > 0

    // Media icons are bundled in the Waffles QRC (CMakeLists RESOURCES) so any consumer gets
    // them for free. Window icons still come from the app's data/ for now (see backlog).
    function media(n) { return "qrc:/qt/qml/Dsqt/Waffles/data/images/waffles/media/" + n }
    function win(n)   { return "file:///%APP%/data/images/waffles/window/" + n }

    // Figma pill widths per type. Used only in `chrome:true` (windowed) mode; when embedded in
    // the fullscreen controller (`chrome:false`) the pill fills its parent layout width.
    readonly property int _pillWidthForType: (mtype === "video") ? DsTheme.dp(356)
                                            : (mtype === "pdf")   ? DsTheme.dp(236)
                                            : (mtype === "web")   ? DsTheme.dp(172)
                                            : 0
    // Notify the host (viewer / fullscreen controller) that the user is interacting, so the
    // controls' idle auto-hide is reset.
    function poke()   { mc.interacted() }

    // Mouse over the controls keeps them awake.
    HoverHandler { onPointChanged: mc.poke() }

    // Slider track/handle styling shared by the scrubber / page / volume sliders. Per Figma the
    // active fill is white (surfaceText), NOT accent — only the loop icon uses accent (when on).
    component Track: Rectangle {
        property Slider slider
        x: slider.leftPadding
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: slider.availableWidth
        height: DsTheme.dp(4)
        radius: DsTheme.dp(2)
        color: DsTheme.track
        Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: DsTheme.dp(2); color: DsTheme.surfaceText }
    }
    component Handle: Rectangle {
        property Slider slider
        x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: DsTheme.dp(12); height: DsTheme.dp(12); radius: DsTheme.dp(6); color: DsTheme.surfaceText
    }

    // Pill — per-type Figma width when standalone, fills the parent when embedded in the FS bar.
    Item {
        id: pill
        height: parent ? parent.height : DsTheme.dp(37)
        width: mc.chrome ? mc._pillWidthForType : mc.width
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter

        DsGlassBackground {
            anchors.fill: parent
            visible: mc.chrome
            fallbackColor: DsTheme.scrim
            borderColor: DsTheme.stroke
            borderWidth: DsTheme.glassBorderWidth
            topLeftRadius: DsTheme.dp(20)
            topRightRadius: DsTheme.dp(20)
            bottomLeftRadius: DsTheme.dp(20)
            bottomRightRadius: DsTheme.dp(20)
        }

        Loader {
            anchors.fill: parent
            anchors.leftMargin: mc.chrome ? DsTheme.dp(12) : 0
            anchors.rightMargin: mc.chrome ? DsTheme.dp(12) : 0
            sourceComponent: mc.mtype === "video" ? videoComp
                           : mc.mtype === "pdf"   ? pdfComp
                           : mc.mtype === "web"   ? webComp : null
        }
    }

    // VIDEO / AUDIO
    Component {
        id: videoComp
        RowLayout {
            spacing: DsTheme.dp(8)
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: (mc.item && mc.item.playbackState === MediaPlayer.PlayingState) ? mc.media("pause.svg") : mc.media("play.svg")
                onClicked: {
                    mc.poke();
                    if (!mc.item) return;
                    if (mc.item.playbackState === MediaPlayer.PlayingState) mc.item.pause(); else mc.item.play();
                }
            }
            Slider {
                id: scrub
                Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter
                from: 0; to: (mc.item && mc.item.duration > 0) ? mc.item.duration : 1
                onMoved: { mc.poke(); if (mc.item && mc.item.seekable) mc.item.position = value }
                onPressedChanged: mc.poke()
                Connections { target: mc.item; enabled: mc.item !== null; function onPositionChanged() { if (!scrub.pressed) scrub.value = mc.item.position } }
                background: Track { slider: scrub }
                handle: Handle { slider: scrub }
            }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: mc.media("loop.svg")
                active: mc.item && mc.item.loops === MediaPlayer.Infinite
                onClicked: { mc.poke(); if (mc.item) mc.item.loops = (mc.item.loops === MediaPlayer.Infinite ? 1 : MediaPlayer.Infinite) }
            }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: mc.media("volume.svg")
                active: mc.item && mc.item.muted
                onClicked: { mc.poke(); if (mc.item) mc.item.muted = !mc.item.muted }
            }
            Slider {
                id: vol
                Layout.preferredWidth: DsTheme.dp(80); Layout.alignment: Qt.AlignVCenter
                from: 0; to: 1; value: mc.item ? mc.item.volume : 1
                onMoved: { mc.poke(); if (mc.item) mc.item.volume = value }
                onPressedChanged: mc.poke()
                background: Track { slider: vol }
                handle: Handle { slider: vol }
            }
        }
    }

    // PDF
    Component {
        id: pdfComp
        RowLayout {
            spacing: DsTheme.dp(8)
            Text {
                Layout.preferredWidth: DsTheme.dp(48); Layout.alignment: Qt.AlignVCenter
                text: mc.item ? ((mc.item.currentPage + 1) + "/" + mc.item.pageCount) : "0/0"
                color: DsTheme.surfaceText
                font.family: "Roboto"; font.pixelSize: DsTheme.dp(12); font.letterSpacing: 0.6
                horizontalAlignment: Text.AlignHCenter
            }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: mc.media("arrow_left.svg")
                enabled: mc.item && mc.item.currentPage > 0
                onClicked: { mc.poke(); if (mc.item) mc.item.goToPage(mc.item.currentPage - 1) }
            }
            Slider {
                id: pageSlider
                Layout.fillWidth: true; Layout.alignment: Qt.AlignVCenter
                from: 0; to: (mc.item && mc.item.pageCount > 1) ? mc.item.pageCount - 1 : 0
                stepSize: 1; snapMode: Slider.SnapAlways
                onMoved: { mc.poke(); if (mc.item) mc.item.goToPage(Math.round(value)) }
                onPressedChanged: mc.poke()
                Connections { target: mc.item; enabled: mc.item !== null; function onCurrentPageChanged() { if (!pageSlider.pressed) pageSlider.value = mc.item.currentPage } }
                background: Track { slider: pageSlider }
                handle: Handle { slider: pageSlider }
            }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: mc.media("arrow_right.svg")
                enabled: mc.item && mc.item.currentPage < mc.item.pageCount - 1
                onClicked: { mc.poke(); if (mc.item) mc.item.goToPage(mc.item.currentPage + 1) }
            }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: (mc.viewer && mc.viewer.contentLocked) ? mc.win("locked.svg") : mc.win("lock.svg")
                active: mc.viewer && mc.viewer.contentLocked
                onClicked: { mc.poke(); if (mc.viewer) mc.viewer.contentLocked = !mc.viewer.contentLocked }
            }
        }
    }

    // WEB
    Component {
        id: webComp
        RowLayout {
            spacing: DsTheme.dp(8)
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: mc.media("keyboard.svg")
                // Toggle the stage's floating virtual keyboard (for typing into web inputs).
                readonly property var _stage: mc.viewer ? mc.viewer.stage : null
                active: _stage ? _stage.floatingKeyboardShown : false
                onClicked: { mc.poke(); if (_stage) _stage.floatingKeyboardShown = !_stage.floatingKeyboardShown; }
            }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: mc.media("arrow_left.svg")
                enabled: mc.item && mc.item.canGoBack
                onClicked: { mc.poke(); if (mc.item) mc.item.goBack() }
            }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: mc.media("arrow_right.svg")
                enabled: mc.item && mc.item.canGoForward
                onClicked: { mc.poke(); if (mc.item) mc.item.goForward() }
            }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: mc.media("reload.svg")
                onClicked: { mc.poke(); if (mc.item) mc.item.reload() }
            }
            Item { Layout.fillWidth: true }
            DsCtrlIcon {
                Layout.preferredWidth: DsTheme.dp(24); Layout.preferredHeight: DsTheme.dp(24); Layout.alignment: Qt.AlignVCenter
                iconSize: DsTheme.dp(16)
                iconName: (mc.viewer && mc.viewer.contentLocked) ? mc.win("locked.svg") : mc.win("lock.svg")
                active: mc.viewer && mc.viewer.contentLocked
                onClicked: { mc.poke(); if (mc.viewer) mc.viewer.contentLocked = !mc.viewer.contentLocked }
            }
        }
    }
}
