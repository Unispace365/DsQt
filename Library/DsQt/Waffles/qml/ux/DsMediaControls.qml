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

    implicitHeight: 64
    height: 64
    // Visible only when the media type has controls and not faded out (so it doesn't take input
    // while hidden). In the fullscreen bar its opacity is forced to 1 and the whole bar fades.
    visible: hasControls && opacity > 0

    function media(n) { return "file:///%APP%/data/images/waffles/media/" + n }
    function win(n)   { return "file:///%APP%/data/images/waffles/window/" + n }
    // Notify the host (viewer / fullscreen controller) that the user is interacting, so the
    // controls' idle auto-hide is reset.
    function poke()   { mc.interacted() }

    // Mouse over the controls keeps them awake.
    HoverHandler { onPointChanged: mc.poke() }

    // Slider track/handle styling shared by the scrubber / page / volume sliders.
    component Track: Rectangle {
        property Slider slider
        x: slider.leftPadding
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: slider.availableWidth
        height: 6
        radius: 3
        color: DsTheme.track
        Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 3; color: DsTheme.accent }
    }
    component Handle: Rectangle {
        property Slider slider
        x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
        y: slider.topPadding + slider.availableHeight / 2 - height / 2
        width: 18; height: 18; radius: 9; color: DsTheme.surfaceText
    }

    // Centred pill (constrained width when standalone; fills when embedded).
    Item {
        id: pill
        height: 64
        width: mc.chrome ? Math.max(0, Math.min(mc.width - 80, 1400)) : mc.width
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter

        DsGlassBackground {
            anchors.fill: parent
            visible: mc.chrome
            fallbackColor: DsTheme.scrim
            borderColor: DsTheme.stroke
            borderWidth: DsTheme.glassBorderWidth
            topLeftRadius: DsTheme.glassRadius
            topRightRadius: DsTheme.glassRadius
            bottomLeftRadius: DsTheme.glassRadius
            bottomRightRadius: DsTheme.glassRadius
        }

        Loader {
            anchors.fill: parent
            anchors.leftMargin: mc.chrome ? 24 : 0
            anchors.rightMargin: mc.chrome ? 16 : 0
            sourceComponent: mc.mtype === "video" ? videoComp
                           : mc.mtype === "pdf"   ? pdfComp
                           : mc.mtype === "web"   ? webComp : null
        }
    }

    // VIDEO / AUDIO
    Component {
        id: videoComp
        RowLayout {
            spacing: 14
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
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
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
                iconName: mc.media("loop.svg")
                active: mc.item && mc.item.loops === MediaPlayer.Infinite
                onClicked: { mc.poke(); if (mc.item) mc.item.loops = (mc.item.loops === MediaPlayer.Infinite ? 1 : MediaPlayer.Infinite) }
            }
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
                iconName: mc.media("volume.svg")
                active: mc.item && mc.item.muted
                onClicked: { mc.poke(); if (mc.item) mc.item.muted = !mc.item.muted }
            }
            Slider {
                id: vol
                Layout.preferredWidth: 160; Layout.alignment: Qt.AlignVCenter
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
            spacing: 14
            Text {
                Layout.preferredWidth: 96; Layout.alignment: Qt.AlignVCenter
                text: mc.item ? ((mc.item.currentPage + 1) + "/" + mc.item.pageCount) : "0/0"
                color: DsTheme.surfaceText
                font.family: "Roboto"; font.pixelSize: 24
            }
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
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
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
                iconName: mc.media("arrow_right.svg")
                enabled: mc.item && mc.item.currentPage < mc.item.pageCount - 1
                onClicked: { mc.poke(); if (mc.item) mc.item.goToPage(mc.item.currentPage + 1) }
            }
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
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
            spacing: 14
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
                iconName: mc.media("keyboard.svg")
                onClicked: { mc.poke(); /* TODO: bring up the on-screen keyboard (larger task) */ }
            }
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
                iconName: mc.media("arrow_left.svg")
                enabled: mc.item && mc.item.canGoBack
                onClicked: { mc.poke(); if (mc.item) mc.item.goBack() }
            }
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
                iconName: mc.media("arrow_right.svg")
                enabled: mc.item && mc.item.canGoForward
                onClicked: { mc.poke(); if (mc.item) mc.item.goForward() }
            }
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
                iconName: mc.media("reload.svg")
                onClicked: { mc.poke(); if (mc.item) mc.item.reload() }
            }
            Item { Layout.fillWidth: true }
            DsCtrlIcon {
                Layout.preferredWidth: 48; Layout.preferredHeight: 48; Layout.alignment: Qt.AlignVCenter
                iconName: (mc.viewer && mc.viewer.contentLocked) ? mc.win("locked.svg") : mc.win("lock.svg")
                active: mc.viewer && mc.viewer.contentLocked
                onClicked: { mc.poke(); if (mc.viewer) mc.viewer.contentLocked = !mc.viewer.contentLocked }
            }
        }
    }
}
