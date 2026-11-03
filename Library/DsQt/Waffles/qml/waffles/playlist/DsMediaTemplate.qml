pragma ComponentBehavior: Bound

import QtQuick
import QtMultimedia
import Dsqt.Core
import Dsqt.Waffles

// Playlist slide template: full-screen media (image / video / pdf / web).
//
// Reads a slide descriptor (from DsContentLauncherModel.slidesFor) and renders its media via the
// Core DsMediaViewer, honouring the schema "Media Template" fields off the raw record:
//   - display_mode : "fill" → Fill (PreserveAspectCrop); anything else → Fit (PreserveAspectFit)
//   - autoplay     : video autoplay (default true)
//   - loop         : loop forever when truthy
//   - page_number  : initial PDF page
//
// This is the first/simplest template. The playlist viewer's registry maps a slide's typeUid to a
// template like this; unmapped slides fall back here (most slides carry a media descriptor).
//
// Template contract (what DsPlaylistViewer drives): `slide` (the descriptor) and `active` (whether
// this is the currently-visible slide) and `paused` (suspends playback without disabling input).
Item {
    id: tmpl

    property var  slide: null
    property bool active: true
    property bool paused: false
    property bool playbackSuspended: false
    readonly property bool _playbackEnabled: tmpl.active && !tmpl.paused && (!tmpl._rec || tmpl._rec.autoplay !== false)

    function _syncPlayback() {
        const player = mv.mediaItem;
        if (!player) return;
        if (tmpl.playbackSuspended && tmpl._playbackEnabled) return;
        if (mv.videoItem) {
            if (tmpl._playbackEnabled) player.play();
            else player.pause();
        } else if ("playing" in player && "paused" in player) {
            // AnimatedImage.paused preserves its frame; playing=false would restart it.
            player.paused = !tmpl._playbackEnabled;
            if (tmpl._playbackEnabled) player.playing = true;
        }
    }
    on_PlaybackEnabledChanged: Qt.callLater(tmpl._syncPlayback)


    readonly property var _rec:   slide ? slide.record : null
    readonly property var _media: slide ? slide.media : null
    readonly property int _fill:  (_rec && ("" + _rec.display_mode).toLowerCase() === "fill")
                                  ? Image.PreserveAspectCrop : Image.PreserveAspectFit

    DsMediaViewer {
        id: mv
        objectName: "mv"
        anchors.fill: parent
        media: tmpl._media
        fillMode: tmpl._fill
        autoPlay: tmpl._playbackEnabled
        playbackSuspended: tmpl.playbackSuspended
        onMediaItemChanged: Qt.callLater(tmpl._syncPlayback)
        onMediaLoaded: tmpl._syncPlayback()
        // Qt 6.12 AnimatedImage treats zero loops as disabled playback.
        loops: (tmpl._rec && tmpl._rec.loop) ? MediaPlayer.Infinite : 1
        page: (tmpl._rec && tmpl._rec.page_number) ? tmpl._rec.page_number : 1
    }
    DsContentState { anchors.fill: parent; viewer: mv }
}
