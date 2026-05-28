import QtQuick
import Dsqt.Waffles

// Base for the controllers DsWaffleStage shows while a viewer is fullscreen (one component per
// viewer type — see DsWaffleStage.fullscreenController / presentationController).
//
// The contract the stage drives:
//   - property var  viewer     : the fullscreen viewer this controller controls (set by the stage)
//   - property bool shown      : fade the controller in / out
//   - property bool collapsed  : reset on show (subclasses may use it for a collapsed state)
//
// Subclasses build their own bar/tab layout and use `glassContext` for their DsGlassBackground
// panels, so the controller chrome is real frosted glass over the fullscreen media (it samples the
// fullscreen viewer's own glass-free content). When glass is off / unavailable the panels fall
// back to a solid colour.
Item {
    id: controller

    property var  viewer: null
    property bool shown: false
    property bool collapsed: false

    readonly property var model: viewer ? viewer.model : null

    // Fade with `shown`. Subclasses keep their own internal layout; this shows/hides the whole thing.
    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

    // Frosted-glass context for the controller's panels. `source` is the fullscreen viewer's
    // glass-free content (captureItem); each DsGlassBackground samples its own slice of it so a
    // panel shows a blurred cut-out of the media directly behind it.
    //
    // Sampling mirrors DsTitledMediaViewer's own glass (the proven pattern): each panel is mapped
    // to `viewerItem` — set to the CONTROLLER, an ancestor of every panel — which yields a stable
    // descendant→ancestor offset, then `viewerX`/`viewerY` translate that into the captureItem's
    // coordinate space. Because captureItem fills the fullscreen viewer, that translation is just
    // (controller stage pos − viewer stage pos). Crucially `viewerX`/`viewerY` are read directly in
    // DsGlassBackground._origin, so they are hard reactive dependencies — the glass re-samples as
    // the controller is dragged WITHOUT relying on cross-subtree mapToItem (which isn't reactive).
    // `refresh` still nudges a re-sample after initial layout / viewer resize.
    readonly property var glassContext: _glassCtx
    QtObject {
        id: _glassCtx
        property Item  source:     controller.viewer ? controller.viewer.captureItem : null
        property Item  viewerItem: controller
        property real  viewerX: controller.x - (controller.viewer ? controller.viewer.x : 0)
        property real  viewerY: controller.y - (controller.viewer ? controller.viewer.y : 0)
        property bool  enabled: controller.viewer
                                ? (("glassEnabled" in controller.viewer) ? controller.viewer.glassEnabled : true)
                                : true
        property color tint:        DsTheme.surface
        property real  tintOpacity: DsTheme.glassTintOpacity
        property real  blur:        DsTheme.glassBlur
        property int   blurMax:     DsTheme.glassBlurMax
        property real  blurMultiplier: DsTheme.glassBlurMultiplier
        property color borderColor: DsTheme.stroke
        property real  borderWidth: DsTheme.glassBorderWidth
        property real  radius:      DsTheme.glassRadius
        property real  refresh: controller.x + controller.y +
            (controller.viewer ? controller.viewer.x + controller.viewer.y
                               + controller.viewer.width + controller.viewer.height : 0)
    }
}
