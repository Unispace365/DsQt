import QtQuick
import QtQuick.Effects

// A frosted-glass background panel. Samples the rectangular region of `source`
// (a stage-sized composite built by DsWaffleStage) at (sampleX, sampleY) sized to
// this item, then blurs + tints + rounds it. sampleX/sampleY are in `source`'s
// coordinate space (stage coordinates), so adjacent panels that share a source line
// up seamlessly. With blurEnabled false (or no source) it falls back to a flat tint,
// which is also the "glass off" appearance.
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

    MultiEffect {
        anchors.fill: parent
        visible: glass._live
        source: grab
        blurEnabled: true
        blur: glass.blur
        blurMax: glass.blurMax
        maskEnabled: true
        maskSource: mask
    }

    // Tint sits over the blur (frosted look); on its own it is the glass-off panel.
    Rectangle {
        anchors.fill: parent
        color: glass.tint
        opacity: glass.tintOpacity
        topLeftRadius: glass.topLeftRadius
        topRightRadius: glass.topRightRadius
        bottomLeftRadius: glass.bottomLeftRadius
        bottomRightRadius: glass.bottomRightRadius
    }
}
