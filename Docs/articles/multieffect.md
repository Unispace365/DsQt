# DsMultiEffect: masking and extending Qt Quick effects

`DsMultiEffect` is part of `Dsqt.Core`. It derives directly from Qt Quick's
`MultiEffect`, retaining its properties, signals, and Qt-provided rendering.
It adds a direct alpha mask, four edge fades, and independent rounded corners.
These additions are disabled by default.

## Use an alpha mask on a Flickable

```qml
import QtQuick
import Dsqt.Core

Item {
    width: 320
    height: 480

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height
        flickableDirection: Flickable.VerticalFlick
        opacity: 0 // Hide the original drawing while retaining pointer input.

        Column {
            id: content
            width: flick.width
            spacing: 8
            Repeater {
                model: 30
                Rectangle {
                    required property int index
                    width: content.width
                    height: 64
                    color: "tomato"
                    Text { anchors.centerIn: parent; text: parent.index }
                }
            }
        }
    }

    Rectangle {
        id: alphaMask
        width: flick.width
        height: flick.height
        visible: false
        layer.enabled: true
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#00ffffff" }
            GradientStop { position: 0.15; color: "#ffffffff" }
            GradientStop { position: 0.85; color: "#ffffffff" }
            GradientStop { position: 1.0; color: "#00ffffff" }
        }
    }

    DsMultiEffect {
        anchors.fill: flick
        source: flick
        opacityMaskEnabled: true
        opacityMaskSource: alphaMask
    }
}
```

Use `opacity: 0` for an interactive source. `visible: false` prevents that item
from receiving pointer input. The effect does not forward input; the original
Flickable remains the input target. Do not place the effect inside its own source
subtree, which would create a rendering dependency cycle.

`opacityMaskSource` must supply a texture: an `Image`, `ShaderEffectSource`, or
an item with `layer.enabled: true`. For an arbitrary item, enable its layer as
above. Give hidden masks nonzero dimensions and keep their geometry updated.
A null mask is a no-op, including when inversion is enabled. A fully transparent
mask is different: it hides the output (or reveals it when inverted).

For simple viewport fades, no separate mask texture is necessary:

```qml
DsMultiEffect {
    anchors.fill: flick
    source: flick
    edgeFadeEnabled: true
    fadeTop: 24
    fadeBottom: 24
    roundedCornersEnabled: true
    cornerRadius: 12
    bottomRightRadius: 0
}
```

It also supports `someItem.layer.effect: DsMultiEffect { ... }`. Set
`someItem.layer.enabled: true` and let Qt inject the effect's `source`; do not
point `source` back at `someItem` in that arrangement.

## API

All original `MultiEffect` properties remain available, including `source`,
brightness, contrast, saturation, colorization, blur, shadows, the original
threshold mask, automatic/custom padding, and the read-only diagnostics.
The `shaderChanged` signal and other inherited signals also remain available.

| Added property | Default | Meaning |
| --- | --- | --- |
| `opacityMaskEnabled: bool` | `false` | Enables direct alpha multiplication when a mask is present. |
| `opacityMaskSource: Item` | `null` | Texture provider sampled by normalized output coordinates. RGB is ignored. |
| `opacityMaskInverted: bool` | `false` | Uses `1 - maskAlpha`. |
| `edgeFadeEnabled: bool` | `false` | Enables the configured edge fades. |
| `fadeTop`, `fadeRight`, `fadeBottom`, `fadeLeft`: `real` | `0` | Distance in logical pixels from that edge to full opacity. |
| `roundedCornersEnabled: bool` | `false` | Enables the configured rounded corners. |
| `cornerRadius: real` | `0` | Default radius in logical pixels for all four corners. |
| `topLeftRadius`, `topRightRadius`, `bottomRightRadius`, `bottomLeftRadius`: `real` | Bound to `cornerRadius` | Per-corner override. |
| `customEffectsEnabled: bool` (read-only) | `false` | Whether the extra render pass is required. |

Negative fade distances and radii act as zero. Each radius is independently
limited to half the shorter rendered dimension. A fade can exceed the item's
size; in that case it never reaches full opacity within the item. Overlapping
fades multiply. Assigning an individual radius replaces its default QML binding
to `cornerRadius`; restore that binding with `Qt.binding()` if needed.

The original `maskEnabled`, `maskSource`, `maskInverted`, `maskThresholdMin`,
`maskThresholdMax`, `maskSpreadAtMin`, and `maskSpreadAtMax` retain Qt's threshold
behavior. They are independent of the new opacity mask and may be combined.
For a pure alpha mask, leave `maskEnabled` disabled and enable
`opacityMaskEnabled` instead. An alpha of 0.25 scales the resulting premultiplied
pixel by 0.25, with no smoothstep remapping.

## Rendering order, coordinates, and cost

The pipeline is:

1. Qt's `MultiEffect` renders its complete output, using Qt's native ordering.
2. If an addition is active, DSQt captures that output into a texture.
3. One fragment shader multiplies that texture by mask alpha, edge coverage,
   and rounded-corner coverage, then applies the outer item opacity.

The additions therefore affect the completed blur and shadow too. To mask an
object before generating its shadow, compose separate effects explicitly.

All additions use the **full rendered rectangle**, `MultiEffect.itemRect`, which
includes automatic blur/shadow padding or `paddingRect`. A mask is stretched
across this rectangle; fades start at its outer edges; corner radii round those
edges. With no padding, this is the item's ordinary `width` and `height`.
The internal effect keeps the padded rectangle's original offset and size so
it does not squeeze a shadow into the unpadded item. Parent clipping can still
clip the effect, as with any Qt Quick item.

There is no custom pass when all additions are inactive. Enabling a real addition
adds an offscreen texture and one full-rectangle draw. A separate layered mask
may add another texture/render. Texture size grows with padded area and device
pixel ratio; avoid unnecessarily large padding. Prefer edge fades over a mask
texture when they express the same shape. Animate values rather than repeatedly
toggling features that create/destroy a layer.

`DsMultiEffect` owns its `layer.enabled`, `layer.sourceRect`, `layer.smooth`, and
`layer.effect` bindings. Do not override them. This is the main restriction
compared with using a bare `MultiEffect`. Properties beginning with `_` are
implementation details. Inherited `fragmentShader`, `vertexShader`,
`hasProxySource`, and `shaderChanged` describe the native Qt stage, not DSQt's
additional layer or mask texture.

Like `MultiEffect` and `ShaderEffect`, this component requires Qt Quick's GPU/RHI
rendering path; the Qt Quick software scene graph does not support these effects.

## Automatic shader compilation

Core's CMake file finds the matching Qt `ShaderTools` package and calls:

```cmake
qt_add_shaders(Core "dsqt_core_effects"
    PREFIX "/dsqt/core"
    FILES shaders/dsMultiEffect.frag
)
```

CMake invokes the kit's `qsb` whenever the GLSL source changes. The generated
bundle contains SPIR-V and the default GLSL, HLSL, and Metal variants. No manual
`qsb` command, runtime shader source compiler, or checked-in `.qsb` is required.
Install Qt Shader Tools in the development kit. Consumers of the built library
use its embedded bundle and do not need to compile it themselves.

DSQt's installed package imports static archives manually. The CMake file also
adds the generated bundle to Core's QML module resources, making the generated
Core plugin retain that resource when linked from installed archives. The
component uses that retained URL:

```text
qrc:/qt/qml/Dsqt/Core/shaders/dsMultiEffect.frag.qsb
```

The `qt_add_shaders` resource and QML resource registrations intentionally use
different prefixes. The small duplicate in build-tree consumers avoids relying
on Qt's private CMake properties or requiring application-side `Q_INIT_RESOURCE`.
Keep the extra module resource registration when changing shader packaging.

## Adding an effect

### 1. Define its contract and place in the pipeline

Decide whether the new operation needs only the current pixel, neighboring
pixels, another texture, or intermediate images. Color transforms and vignette
can share the existing pass. A multi-stage blur or other neighborhood operation
may need a separate pass. Do not force all effects into one shader if that changes
their semantics or makes inactive effects expensive.

Specify the defaults, units, range/clamping, identity value, coordinate space,
and interaction with padding. State whether it operates before or after the
native blur/shadow and whether it changes coverage or color. Preserve all current
property names and keep new features disabled by default.

### 2. Add public QML properties and activation logic

Edit `Library/DsQt/Core/qml/DsMultiEffect.qml`. For example, a future vignette
could expose:

```qml
property bool vignetteEnabled: false
property real vignetteStrength: 0
readonly property bool _vignetteActive: vignetteEnabled && vignetteStrength > 0
```

Include `_vignetteActive` in `customEffectsEnabled`. Add a property on the inner
`ShaderEffect`, such as:

```qml
property real vignetteAmount: effect._vignetteActive
    ? Math.min(1, Math.max(0, effect.vignetteStrength)) : 0
```

This example is an extension recipe, not an existing API. Bind uniforms to the
root `effect` explicitly; do not imperatively copy properties in change handlers.
Send identity values when disabled. Ensure a zero-strength setting can bypass
the layer if no other custom feature needs it.

### 3. Extend the shader interface

Edit `Library/DsQt/Core/shaders/dsMultiEffect.frag`. Add a matching
`float vignetteAmount;` to the `std140` uniform block, and use
`ubuf.vignetteAmount` in GLSL. The QML property name and uniform name/type must
match. Keep `qt_Matrix` first and `qt_Opacity` immediately after it so the default
Qt Quick vertex shader remains compatible. Let qsb reflection handle packing;
`vec2`, `vec3`, and `vec4` have alignment requirements that differ from a compact
C++ structure.

Texture samplers belong outside the block and need unique bindings after zero;
`source` currently uses 1 and `alphaMask` uses 2. A new input texture needs a
matching QML property and a documented texture-provider contract. Null inputs
must have an explicit no-op or fallback path, and must not be sampled to infer
whether the feature is enabled.

### 4. Preserve premultiplied alpha

For a coverage operation, multiply **all four channels**:

```glsl
pixel *= coverage;
```

Changing alpha alone produces incorrect blending and colored fringes. For a
color operation defined in straight RGB, first guard against zero alpha, divide
RGB by alpha, transform it, and multiply by alpha again. Do not unpremultiply for
operations that are already valid in premultiplied form. Specify whether color
math is in the sampled color space or requires a linear-light conversion.

A vignette that darkens RGB without changing opacity might use normalized
coordinates and `pixel.rgb *= 1.0 - amount * falloff`. Define the exact falloff,
aspect-ratio behavior, and order relative to other color effects before adding it.
Apply `qt_Opacity` only once at the final output. Clamp invalid public inputs and
avoid divisions by zero when dimensions or alpha are zero.

### 5. Rebuild and test observable output

The existing CMake rule rebuilds changes to this shader automatically. A new
shader file must be added to `qt_add_shaders` and, for installed static-library
use, to the retained QML resource list with the correct alias/URL.

Extend `Library/Tests/Effects/qml/tst_multieffect.qml` with pixel comparisons for
identity/disabled behavior, useful non-default values, live updates, transparency,
combined features, padding, resizing, and small/zero geometry. Use expected
results derived from the effect's contract, not a copy of its implementation.
Keep the original native-parity tests: an opaque new mask must retain the native
blur, color, and shadow output including margins. Test input delivery when a
change affects the source/capture setup. Treat QML and shader warnings as failures.

Run on the supported Qt versions and renderer backends. qsb translating a shader
successfully does not prove that it renders correctly on every GPU. Check a high
DPI setting and representative opaque/transparent content. When packaging changes,
run against an installed build as well: extra build-tree resource targets can
conceal missing resource retention in static archives.

### 6. Document and measure

Update this guide's API table, ordering and cost notes, and examples. State whether
the new operation adds a texture, a pass, or additional texture samples. Measure
representative scenes if it changes per-pixel work substantially. Keep defaults
visually identical to `MultiEffect`.

## Running the rendering tests

The normal library build includes this suite with `-DDSQT_BUILD_TESTS=ON`.
There is also a standalone entry point that builds the real Core module without
the unrelated media SDKs. From a compiler-enabled terminal, with Qt, Ninja, and
tomlplusplus available:

```powershell
cmake -S Library/Tests/Effects -B Library/build/effects -G Ninja `
    -DCMAKE_BUILD_TYPE=Debug `
    "-DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/msvc2022_64;C:/path/to/vcpkg_installed/x64-windows"
cmake --build Library/build/effects --target test_ds_multieffect
ctest --test-dir Library/build/effects --output-on-failure
```

Put the chosen Qt kit's `bin` and the matching dependency DLL directories on
`PATH` before running. Use an available RHI backend, for example
`$env:QSG_RHI_BACKEND = "d3d11"` on Windows. A display capable of rendering Qt Quick
is needed. To capture detailed output directly, run the test executable with
`-o results.txt,txt`.

To exercise an already installed DsQt package, configure a separate build with
`-DDSQT_EFFECTS_USE_INSTALLED=ON` and include its prefix in `CMAKE_PREFIX_PATH`.
The runner deliberately does not call `Q_INIT_RESOURCE`; the Core plugin must
retain the shader by itself.

## Qt references

- [MultiEffect API and source handling](https://doc.qt.io/qt-6/qml-qtquick-effects-multieffect.html)
- [ShaderEffect uniforms and premultiplied color](https://doc.qt.io/qt-6/qml-qtquick-shadereffect.html)
- [qt_add_shaders and qsb generation](https://doc.qt.io/qt-6/qt-add-shaders.html)
- [Qt Quick Test image comparisons](https://doc.qt.io/qt-6/qml-qttest-testcase.html)
