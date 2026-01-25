# Qt RHI Shaders for TouchEngine

This directory contains shader files for rendering TouchEngine textures using Qt's RHI (Rendering Hardware Interface).

## Files

- **texture.vert** - Vertex shader (GLSL 4.4)
- **texture.frag** - Fragment shader (GLSL 4.4)
- **texture.vert.qsb** - Compiled vertex shader (generated)
- **texture.frag.qsb** - Compiled fragment shader (generated)

## Compiling Shaders

Qt RHI requires shaders to be compiled into `.qsb` (Qt Shader Baker) format, which contains multiple shader variants for different graphics APIs.

### Windows

```bash
compile_shaders.bat
```

### Linux/macOS

```bash
chmod +x compile_shaders.sh
./compile_shaders.sh
```

### Manual Compilation

If you need to compile manually:

```bash
# Update path to your Qt installation
qsb --glsl "100 es,120,150" --hlsl 50 --msl 12 \
    -o texture.vert.qsb texture.vert

qsb --glsl "100 es,120,150" --hlsl 50 --msl 12 \
    -o texture.frag.qsb texture.frag
```

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

The QSB files contain shader variants for:

- **GLSL ES 100** - OpenGL ES 2.0 (mobile)
- **GLSL 120** - OpenGL 2.1 (legacy desktop)
- **GLSL 150** - OpenGL 3.2+ (modern desktop)
- **HLSL 50** - Direct3D 11/12 (Windows)
- **MSL 12** - Metal 1.2 (macOS/iOS)

This ensures the shaders work across all Qt RHI backends.

## Note

The current TouchEngine implementation uses Qt's Scene Graph directly via `QSGSimpleTextureNode`, which handles rendering automatically. These shaders are provided for reference and potential custom rendering implementations.

If you want to implement custom shader effects (color correction, compositing, etc.), you can modify these shaders as a starting point.

## Troubleshooting

### "qsb not found"
- Ensure Qt 6.9.2+ is installed
- Update the Qt path in the compile scripts
- The qsb tool is located in `<Qt_Dir>/bin/`

### Shader compilation errors
- Verify GLSL syntax is correct
- Check that shader versions match (440 core)
- Ensure uniform blocks use std140 layout

### Runtime shader errors
- Check that .qsb files are in the resources directory
- Verify Qt RHI backend supports the shader variants
- Look for warnings in application console output