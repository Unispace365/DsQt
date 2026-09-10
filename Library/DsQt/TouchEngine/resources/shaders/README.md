# Qt RHI Shaders for TouchEngine

This directory contains shader files for rendering TouchEngine textures using Qt's RHI (Rendering Hardware Interface).

## Files

- **texture.vert** - Vertex shader (GLSL 4.4)
- **texture.frag** - Fragment shader (GLSL 4.4)

## Compiling Shaders

Qt RHI requires `.qsb` shader packs containing variants for each enabled
graphics API. `qt_add_shaders()` in the module's `CMakeLists.txt` generates and
embeds those packs during the normal build. Generated `.qsb` files are build
artifacts and are not checked into this directory.

## What the Shaders Do

These are simple texture rendering shaders:

### Vertex Shader (`texture.vert`)
- Takes vertex position and texture coordinates
- Applies MVP (Model-View-Projection) matrix
- Passes texture coordinates to fragment shader

### Fragment Shader (`texture.frag`)
- Samples the TouchEngine texture
- Outputs the color directly (no modifications)

## Shader Variants

The generated packs cover the Windows RHI backends supported by this module:
Direct3D 11, Direct3D 12, Vulkan, and desktop OpenGL. TouchEngine-Windows does
not provide a Metal backend.

## Note

`DsTouchEngineView` uses these shaders from its `QQuickRhiItemRenderer` to draw
TouchEngine outputs and to normalize QML texture-provider inputs into
backend-independent RHI textures.

If you want to implement custom shader effects (color correction, compositing, etc.), you can modify these shaders as a starting point.

## Troubleshooting

### "qsb not found"
- Ensure Qt 6.9 or newer, including the ShaderTools component, is installed.
- Reconfigure the CMake build so it can locate Qt's `qsb` tool.

### Shader compilation errors
- Verify GLSL syntax is correct
- Check that shader versions match (440 core)
- Ensure uniform blocks use std140 layout

### Runtime shader errors
- Verify the generated shader resource target is linked by the consumer.
- Verify the selected Qt RHI backend is D3D11, D3D12, Vulkan, or OpenGL.
- Check the application log for `Dsqt.TouchEngine` diagnostics.
