import QtQuick
import QtQuick.Effects

// A frosted-glass background panel. Samples the region of `source` (a stage-sized composite
// built by DsWaffleStage) at (sampleX, sampleY) sized to this item, then blurs + tints + rounds
// it. sampleX/sampleY are in `source`'s coordinate space (stage coordinates); the caller
// computes them from reactive properties (e.g. viewer.x + local offset) so the panel tracks
// dragging and reordering.
//
// Glass-off path: when blurEnabled is false (or there is no source) the panel falls back to a
// developer-supplied `fallbackComponent` if set, otherwise a solid `fallbackColor`. This lets
// each panel — including control backgrounds — handle the no-glass case differently.
Item {
    id: glass

    property Item  source: null
    property real  sampleX: 0
    property real  sampleY: 0

    property bool  blurEnabled: true
    property color tint: "#191F25"
    property real  tintOpacity: 0.8
    property real  blur: 0.5
    property int   blurMax: 32

    property color fallbackColor: tint
    property Component fallbackComponent: null

    property real  topLeftRadius: 0
    property real  topRightRadius: 0
    property real  bottomLeftRadius: 0
    property real  bottomRightRadius: 0

    readonly property bool _live: glass.blurEnabled && glass.source !== null

    ShaderEffectSource {
        id: grab
        anchors.fill: parent
        visible: false
        sourceItem: glass.source
        sourceRect: Qt.rect(glass.sampleX, glass.sampleY, glass.width, glass.height)
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
        // Auto-padding inflates the effect rect for the blur spread, which scales/offsets the
        // sampled content — keep it off so the blur maps 1:1 onto the panel.
        autoPaddingEnabled: false
        blurEnabled: true
        blur: glass.blur
        blurMax: glass.blurMax
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
}
