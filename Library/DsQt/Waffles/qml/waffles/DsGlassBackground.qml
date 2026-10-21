import QtQuick
import QtQuick.Effects

// The reusable frosted-glass background element. A viewer region OR a control set drops this in
// "the same basic way": set `context` (provided by the viewer to its control sets via the `glass`
// property) plus this panel's size/radii, and it samples the shared backdrop slot at its own
// location and blurs + tints + rounds + outlines it.
//
// Self-sampling: with a `viewerItem` (and the viewer's stage x/y + a `refresh` tick) the panel
// works out which slice of `source` sits behind it via mapToItem — no caller-side coordinate
// math. If `viewerItem` is null it falls back to explicit `sampleX`/`sampleY`.
//
// Glass-off path: when blurEnabled is false (or there is no source) it shows a developer-supplied
// `fallbackComponent` if set, else a solid `fallbackColor`.
Item {
    id: glass

    // Optional bundle of {source, viewerItem, viewerX, viewerY, refresh, enabled, tint,
    // tintOpacity, blur, blurMax, borderColor, borderWidth}. Individual properties below default
    // from it, and can still be overridden directly.
    property var context: null

    property Item  source:      context ? context.source : null
    property Item  viewerItem:  context ? context.viewerItem : null
    property real  viewerX:     context ? context.viewerX : 0
    property real  viewerY:     context ? context.viewerY : 0
    property real  refresh:     context ? context.refresh : 0
    property real  sampleX: 0
    property real  sampleY: 0

    property bool  blurEnabled: context ? context.enabled : true
    property color tint:        context ? context.tint : "#191F25"
    property real  tintOpacity: context ? context.tintOpacity : 0.8
    property real  blur:        context ? context.blur : 0.5
    property int   blurMax:     context ? context.blurMax : 32
    // MultiEffect's blur multiplier — extends the blur radius beyond blurMax without extra
    // texture lookups (cheaper than raising blurMax, at some quality loss). Default 0 disables.
    property real  blurMultiplier: context && ("blurMultiplier" in context) ? context.blurMultiplier : 0

    property color fallbackColor: tint
    property Component fallbackComponent: null

    property color borderColor: context ? context.borderColor : "transparent"
    property real  borderWidth: context ? context.borderWidth : 0

    property real  topLeftRadius: 0
    property real  topRightRadius: 0
    property real  bottomLeftRadius: 0
    property real  bottomRightRadius: 0

    readonly property bool _live: glass.blurEnabled && glass.source !== null
    readonly property point _origin: {
        if (!glass.viewerItem)
            return Qt.point(glass.sampleX, glass.sampleY)
        // Depend on refresh (viewer move/resize) + own geometry so the mapped slice tracks.
        let _ = glass.refresh + glass.x + glass.y + glass.width + glass.height
        let p = glass.mapToItem(glass.viewerItem, 0, 0)
        return Qt.point(glass.viewerX + p.x, glass.viewerY + p.y)
    }

    ShaderEffectSource {
        id: grab
        anchors.fill: parent
        visible: false
        sourceItem: glass.source
        sourceRect: Qt.rect(glass._origin.x, glass._origin.y, glass.width, glass.height)
        live: glass._live
        hideSource: false
    }

    Item {
        id: mask
        anchors.fill: parent
        layer.enabled: true
        visible: false
        Rectangle {
            anchors.fill: parent
            color: "black"
            topLeftRadius: glass.topLeftRadius
            topRightRadius: glass.topRightRadius
            bottomLeftRadius: glass.bottomLeftRadius
            bottomRightRadius: glass.bottomRightRadius
        }
    }

    // ON: blurred backdrop + tint overlay.
    MultiEffect {
        anchors.fill: parent
        visible: glass._live
        source: grab
        autoPaddingEnabled: false
        blurEnabled: true
        blur: glass.blur
        blurMax: glass.blurMax
        blurMultiplier: glass.blurMultiplier
        maskEnabled: true
        maskSource: mask
    }
    Rectangle {
        anchors.fill: parent
        visible: glass._live
        color: glass.tint
        opacity: glass.tintOpacity
        topLeftRadius: glass.topLeftRadius
        topRightRadius: glass.topRightRadius
        bottomLeftRadius: glass.bottomLeftRadius
        bottomRightRadius: glass.bottomRightRadius
    }

    // OFF: developer-provided fallback, else a solid colour.
    Loader {
        anchors.fill: parent
        active: !glass._live && glass.fallbackComponent !== null
        visible: active
        sourceComponent: glass.fallbackComponent
    }
    Rectangle {
        anchors.fill: parent
        visible: !glass._live && glass.fallbackComponent === null
        color: glass.fallbackColor
        topLeftRadius: glass.topLeftRadius
        topRightRadius: glass.topRightRadius
        bottomLeftRadius: glass.bottomLeftRadius
        bottomRightRadius: glass.bottomRightRadius
    }

    // Border outline on top of everything (the design's 1px Tonal-10 stroke).
    Rectangle {
        anchors.fill: parent
        visible: glass.borderWidth > 0
        color: "transparent"
        border.color: glass.borderColor
        border.width: glass.borderWidth
        topLeftRadius: glass.topLeftRadius
        topRightRadius: glass.topRightRadius
        bottomLeftRadius: glass.bottomLeftRadius
        bottomRightRadius: glass.bottomRightRadius
    }
}
