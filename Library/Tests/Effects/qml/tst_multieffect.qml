pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Effects
import QtTest
import Dsqt.Core

Item {
    id: root
    width: 256
    height: 256

    Component {
        id: sceneComponent
        Item {
            id: scene
            width: 256
            height: 256
            property alias sourceItem: sourceItem
            property alias alphaMask: alphaMask
            property alias effect: effect
            property alias reference: reference

            Rectangle {
                id: sourceItem
                x: 64
                y: 64
                width: 128
                height: 128
                opacity: 0
                color: "white"
                layer.enabled: true
            }
            Rectangle {
                id: alphaMask
                width: 128
                height: 128
                color: "white"
                visible: false
                layer.enabled: true
            }
            DsMultiEffect {
                id: effect
                anchors.fill: sourceItem
                source: sourceItem
                opacityMaskSource: alphaMask
            }
            MultiEffect {
                id: reference
                anchors.fill: sourceItem
                source: sourceItem
                visible: false
            }
        }
    }

    Component {
        id: gradientComponent
        Gradient {
            GradientStop { position: 0; color: "#00000000" }
            GradientStop { position: 1; color: "#ffffffff" }
        }
    }

    Component {
        id: layeredComponent
        Item {
            width: 256
            height: 256
            Rectangle {
                x: 64
                y: 64
                width: 128
                height: 128
                color: "red"
                layer.enabled: true
                layer.effect: DsMultiEffect {
                    id: layerEffect
                    edgeFadeEnabled: true
                    fadeTop: 64
                }
            }
        }
    }

    Component {
        id: flickableComponent
        Item {
            id: flickScene
            width: 256
            height: 256
            property alias flick: flick
            Flickable {
                id: flick
                x: 64
                y: 64
                width: 128
                height: 128
                opacity: 0
                contentWidth: width
                contentHeight: 512
                boundsBehavior: Flickable.StopAtBounds
                Rectangle { width: 128; height: 256; color: "red" }
                Rectangle { y: 256; width: 128; height: 256; color: "blue" }
            }
            DsMultiEffect {
                anchors.fill: flick
                source: flick
                edgeFadeEnabled: true
                fadeTop: 32
                fadeBottom: 32
            }
        }
    }

    TestCase {
        id: tests
        name: "DsMultiEffect"
        when: windowShown

        function initTestCase() {
            verify(effectsShaderAvailable, "Core's embedded shader is available")
            // grabImage captures the window, so its clear color must retain alpha.
            root.Window.window.color = "transparent"
        }

        function init() {
            failOnWarning(/.*/)
        }

        function makeScene() {
            const scene = createTemporaryObject(sceneComponent, root)
            verify(scene !== null)
            verify(waitForRendering(scene))
            return scene
        }

        function capture(scene) {
            verify(waitForRendering(scene))
            return grabImage(scene)
        }

        function near(actual, expected, message, tolerance = 3) {
            verify(Math.abs(actual - expected) <= tolerance,
                   message + ": expected " + expected + ", got " + actual)
        }

        function pixelAlpha(image, x, y) {
            return image.alpha(Math.floor(x * image.width / 256),
                               Math.floor(y * image.height / 256))
        }

        function assertSameImage(first, second, tolerance = 2) {
            compare(first.width, second.width)
            compare(first.height, second.height)
            // Include the padded margins as well as the source rectangle.
            const step = Math.max(1, Math.round(3 * first.width / 256))
            for (let y = 0; y < first.height; y += step) {
                for (let x = 0; x < first.width; x += step) {
                    near(first.alpha(x, y), second.alpha(x, y), "alpha " + x + "," + y, tolerance)
                    near(first.red(x, y), second.red(x, y), "red " + x + "," + y, tolerance)
                    near(first.green(x, y), second.green(x, y), "green " + x + "," + y, tolerance)
                    near(first.blue(x, y), second.blue(x, y), "blue " + x + "," + y, tolerance)
                }
            }
        }

        function test_alpha_data() {
            return [
                { tag: "transparent", alpha: 0, inverted: false, expected: 0 },
                { tag: "quarter", alpha: 0.25, inverted: false, expected: 64 },
                { tag: "half", alpha: 0.5, inverted: false, expected: 128 },
                { tag: "opaque", alpha: 1, inverted: false, expected: 255 },
                { tag: "inverted", alpha: 0.25, inverted: true, expected: 191 }
            ]
        }

        function test_alpha(data) {
            const scene = makeScene()
            scene.alphaMask.color = Qt.rgba(1, 0, 0, data.alpha)
            scene.effect.opacityMaskEnabled = true
            scene.effect.opacityMaskInverted = data.inverted
            const image = capture(scene)
            near(pixelAlpha(image, 128, 128), data.expected, "direct mask alpha")
        }

        function test_linearGradientAndLiveUpdates() {
            const scene = makeScene()
            scene.alphaMask.gradient = createTemporaryObject(gradientComponent, scene)
            scene.effect.opacityMaskEnabled = true
            const image = capture(scene)
            for (let offset of [16, 32, 64, 96, 112])
                near(pixelAlpha(image, 128, 64 + offset), 255 * (offset + 0.5) / 128, "linear gradient")
            scene.alphaMask.gradient = null
            scene.alphaMask.color = Qt.rgba(0, 1, 0, 0.25)
            near(pixelAlpha(capture(scene), 128, 128), 64, "updated mask")
        }

        function test_premultiplicationAndInheritedOpacity() {
            const scene = makeScene()
            scene.sourceItem.color = Qt.rgba(1, 0.5, 0.25, 0.5)
            scene.alphaMask.color = Qt.rgba(0, 0, 1, 0.5)
            scene.effect.opacityMaskEnabled = true
            scene.effect.opacity = 0.5
            const image = capture(scene)
            near(pixelAlpha(image, 128, 128), 32, "source alpha times mask alpha times item opacity")
            const color = image.pixel(Math.floor(image.width / 2), Math.floor(image.height / 2))
            near(color.r * 255, 32, "premultiplied red")
            near(color.g * 255, 16, "premultiplied green")
            near(color.b * 255, 8, "premultiplied blue")
        }

        function test_disabledAndNullMask() {
            const scene = makeScene()
            scene.alphaMask.color = "transparent"
            scene.effect.fadeTop = 128
            scene.effect.cornerRadius = 64
            compare(scene.effect.customEffectsEnabled, false)
            near(pixelAlpha(capture(scene), 65, 65), 255, "disabled additions")
            scene.effect.opacityMaskEnabled = true
            near(pixelAlpha(capture(scene), 128, 128), 0, "enabled mask")
            scene.effect.opacityMaskSource = null
            compare(scene.effect.customEffectsEnabled, false)
            near(pixelAlpha(capture(scene), 128, 128), 255, "null mask is a no-op")
            scene.effect.opacityMaskSource = scene.alphaMask
            scene.alphaMask.color = "white"
            near(pixelAlpha(capture(scene), 128, 128), 255, "mask restored")
        }

        function test_fades_data() {
            return [
                { tag: "top", propertyName: "fadeTop", x: 128, y: 80 },
                { tag: "right", propertyName: "fadeRight", x: 175, y: 128 },
                { tag: "bottom", propertyName: "fadeBottom", x: 128, y: 175 },
                { tag: "left", propertyName: "fadeLeft", x: 80, y: 128 }
            ]
        }

        function test_fades(data) {
            const scene = makeScene()
            scene.effect.edgeFadeEnabled = true
            scene.effect[data.propertyName] = 64
            const image = capture(scene)
            near(pixelAlpha(image, data.x, data.y), 255 * 16.5 / 64, "fade distance")
            near(pixelAlpha(image, 128, 128), 255, "unfaded center", 3)
        }

        function test_overlappingFadesAndMask() {
            const scene = makeScene()
            scene.alphaMask.color = Qt.rgba(1, 1, 1, 0.5)
            scene.effect.opacityMaskEnabled = true
            scene.effect.edgeFadeEnabled = true
            scene.effect.fadeTop = 128
            scene.effect.fadeLeft = 128
            near(pixelAlpha(capture(scene), 96, 96), 255 * 0.5 * Math.pow(32.5 / 128, 2),
                 "mask and fades multiply")
        }

        function test_corners_data() {
            return [
                { tag: "top-left", radius: "topLeftRadius", x: 65, y: 65, oppositeX: 190, oppositeY: 190 },
                { tag: "top-right", radius: "topRightRadius", x: 190, y: 65, oppositeX: 65, oppositeY: 190 },
                { tag: "bottom-right", radius: "bottomRightRadius", x: 190, y: 190, oppositeX: 65, oppositeY: 65 },
                { tag: "bottom-left", radius: "bottomLeftRadius", x: 65, y: 190, oppositeX: 190, oppositeY: 65 }
            ]
        }

        function test_corners(data) {
            const scene = makeScene()
            scene.effect.roundedCornersEnabled = true
            scene.effect[data.radius] = 32
            const image = capture(scene)
            near(pixelAlpha(image, data.x, data.y), 0, "selected corner clipped")
            near(pixelAlpha(image, data.oppositeX, data.oppositeY), 255, "other corner retained")
            near(pixelAlpha(image, 128, 128), 255, "center retained")
        }

        function test_radiusClampingAndAntialiasing() {
            const scene = makeScene()
            scene.effect.roundedCornersEnabled = true
            scene.effect.cornerRadius = 1000
            const image = capture(scene)
            near(pixelAlpha(image, 65, 65), 0, "oversized radius is bounded")
            near(pixelAlpha(image, 128, 128), 255, "oversized radius keeps center")
            let partial = false
            for (let y = 0; y < image.height && !partial; ++y) {
                for (let x = 0; x < image.width; ++x) {
                    const alpha = image.alpha(x, y)
                    if (alpha > 10 && alpha < 245) {
                        partial = true
                        break
                    }
                }
            }
            verify(partial, "rounded edge contains antialiased pixels")
            scene.effect.cornerRadius = -12
            scene.effect.edgeFadeEnabled = true
            scene.effect.fadeTop = -20
            compare(scene.effect.customEffectsEnabled, false)
            near(pixelAlpha(capture(scene), 65, 65), 255, "negative values act as zero")
        }

        function test_thresholdMaskStillWorks() {
            const scene = makeScene()
            scene.alphaMask.gradient = createTemporaryObject(gradientComponent, scene)
            scene.effect.maskSource = scene.alphaMask
            scene.effect.maskEnabled = true
            scene.effect.maskThresholdMin = 0.5
            scene.effect.opacityMaskEnabled = true
            const image = capture(scene)
            near(pixelAlpha(image, 128, 80), 0, "native threshold rejects lower alpha")
            near(pixelAlpha(image, 128, 160), 192, "direct alpha multiplies threshold result")
        }

        function test_nativeParity_data() {
            return [
                { tag: "color", settings: { brightness: 0.1, contrast: 0.2, saturation: -0.4,
                                            colorization: 0.3, colorizationColor: "#4080ff" } },
                { tag: "blur-and-shadow", settings: { blurEnabled: true, blur: 0.6, blurMax: 16,
                       shadowEnabled: true, shadowBlur: 0.5, shadowColor: "#8080ff",
                       shadowOpacity: 0.8, shadowHorizontalOffset: 10, shadowVerticalOffset: 7 } },
                { tag: "manual-padding", settings: { autoPaddingEnabled: false,
                       paddingRect: Qt.rect(12, 8, 18, 16), shadowEnabled: true,
                       shadowHorizontalOffset: 6, shadowVerticalOffset: 4, shadowBlur: 0.3 } }
            ]
        }

        function test_nativeParity(data) {
            const scene = makeScene()
            scene.sourceItem.color = "#ff8040"
            for (let key of Object.keys(data.settings)) {
                scene.effect[key] = data.settings[key]
                scene.reference[key] = data.settings[key]
            }
            const disabled = capture(scene)
            scene.effect.visible = false
            scene.reference.visible = true
            const expected = capture(scene)
            assertSameImage(disabled, expected)
            scene.reference.visible = false
            scene.effect.visible = true
            scene.effect.opacityMaskEnabled = true
            const opaqueMask = capture(scene)
            assertSameImage(opaqueMask, expected, 3)
        }

        function test_resizeAndZeroSize() {
            const scene = makeScene()
            scene.effect.edgeFadeEnabled = true
            scene.effect.fadeTop = 32
            scene.sourceItem.width = 0
            scene.sourceItem.height = 0
            wait(0)
            scene.sourceItem.width = 80
            scene.sourceItem.height = 96
            const image = capture(scene)
            near(pixelAlpha(image, 100, 80), 255 * 16.5 / 32, "fade remains measured in pixels")
            near(pixelAlpha(image, 150, 128), 0, "resized bounds")
        }

        function test_layerEffectUsage() {
            const scene = createTemporaryObject(layeredComponent, root)
            verify(scene !== null)
            const image = capture(scene)
            near(pixelAlpha(image, 128, 80), 255 * 16.5 / 64, "layer.effect source injection")
            near(pixelAlpha(image, 128, 160), 255, "layer.effect opaque region")
        }

        function test_flickableStaysInteractiveAndUpdates() {
            const scene = createTemporaryObject(flickableComponent, root)
            verify(scene !== null)
            const before = capture(scene)
            compare(before.pixel(Math.floor(before.width / 2), Math.floor(before.height / 2)).r, 1)
            mouseDrag(scene.flick, 64, 100, 0, -70, Qt.LeftButton, Qt.NoModifier, 200)
            verify(scene.flick.contentY > 0, "opacity-zero source receives the drag")
            scene.flick.cancelFlick()
            scene.flick.contentY = 280
            const after = capture(scene)
            compare(after.pixel(Math.floor(after.width / 2), Math.floor(after.height / 2)).b, 1)
            near(pixelAlpha(after, 128, 80), 255 * 16.5 / 32, "fade stays fixed to viewport")
        }
    }
}