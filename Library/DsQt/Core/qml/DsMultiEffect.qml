pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects

// The inherited MultiEffect renders first. All DSQt additions multiply its
// completed, premultiplied output, including any shadow and padding.
MultiEffect {
    id: effect

    property bool opacityMaskEnabled: false
    property Item opacityMaskSource: null
    property bool opacityMaskInverted: false

    property bool edgeFadeEnabled: false
    property real fadeTop: 0
    property real fadeRight: 0
    property real fadeBottom: 0
    property real fadeLeft: 0

    property bool roundedCornersEnabled: false
    property real cornerRadius: 0
    property real topLeftRadius: cornerRadius
    property real topRightRadius: cornerRadius
    property real bottomRightRadius: cornerRadius
    property real bottomLeftRadius: cornerRadius

    readonly property bool customEffectsEnabled: _maskActive || _fadeActive || _cornersActive

    readonly property bool _maskActive: opacityMaskEnabled && opacityMaskSource !== null
    readonly property bool _fadeActive: edgeFadeEnabled
        && (fadeTop > 0 || fadeRight > 0 || fadeBottom > 0 || fadeLeft > 0)
    readonly property bool _cornersActive: roundedCornersEnabled
        && (topLeftRadius > 0 || topRightRadius > 0 || bottomRightRadius > 0 || bottomLeftRadius > 0)
    readonly property rect _renderRect: itemRect.width > 0 && itemRect.height > 0
        ? itemRect : Qt.rect(0, 0, width, height)

    // DsMultiEffect owns this layer. Keep the padded output at its original
    // position: a layer's sourceRect alone would stretch it into width/height.
    layer.enabled: customEffectsEnabled
    layer.sourceRect: _renderRect
    layer.smooth: true
    layer.effect: Item {
        id: postProcess
        property var source

        ShaderEffect {
            x: effect._renderRect.x
            y: effect._renderRect.y
            width: effect._renderRect.width
            height: effect._renderRect.height

            property var source: postProcess.source
            property var alphaMask: effect.opacityMaskSource
            property real useOpacityMask: effect._maskActive ? 1 : 0
            property real invertOpacityMask: effect.opacityMaskInverted ? 1 : 0
            property vector2d effectSize: Qt.vector2d(width, height)
            property vector4d edgeFade: effect._fadeActive
                ? Qt.vector4d(Math.max(0, effect.fadeTop), Math.max(0, effect.fadeRight),
                              Math.max(0, effect.fadeBottom), Math.max(0, effect.fadeLeft))
                : Qt.vector4d(0, 0, 0, 0)
            property vector4d cornerRadii: effect._cornersActive
                ? Qt.vector4d(Math.max(0, effect.topLeftRadius), Math.max(0, effect.topRightRadius),
                              Math.max(0, effect.bottomRightRadius), Math.max(0, effect.bottomLeftRadius))
                : Qt.vector4d(0, 0, 0, 0)

            fragmentShader: "qrc:/qt/qml/Dsqt/Core/shaders/dsMultiEffect.frag.qsb"
        }
    }
}