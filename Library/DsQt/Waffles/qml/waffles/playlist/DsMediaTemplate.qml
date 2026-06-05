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
// this is the currently-visible slide — gates video playback so off-screen slides don't play).
Item {
    id: tmpl

    property var  slide: null
    property bool active: true

    readonly property var _rec:   slide ? slide.record : null
    readonly property var _media: slide ? slide.media : null
    readonly property int _fill:  (_rec && ("" + _rec.display_mode).toLowerCase() === "fill")
                                  ? Image.PreserveAspectCrop : Image.PreserveAspectFit

    DsMediaViewer {
        id: mv
        anchors.fill: parent
        media: tmpl._media
        fillMode: tmpl._fill
        autoPlay: tmpl.active && (tmpl._rec ? (tmpl._rec.autoplay !== false) : true)
        loops: (tmpl._rec && tmpl._rec.loop) ? MediaPlayer.Infinite : 0
        page: (tmpl._rec && tmpl._rec.page_number) ? tmpl._rec.page_number : 1
    }
}
