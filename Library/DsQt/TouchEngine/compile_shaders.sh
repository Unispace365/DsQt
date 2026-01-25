#!/bin/bash
# Compile shaders to Qt Shader Baker (.qsb) format
# Requires qsb tool from Qt installation

echo "Compiling Qt RHI shaders..."

# Update this path to match your Qt installation
QT_DIR="/opt/Qt/6.9.2/gcc_64"
QSB="$QT_DIR/bin/qsb"

if [ ! -f "$QSB" ]; then
    echo "ERROR: qsb not found at $QSB"
    echo "Please update QT_DIR path in this script"
    exit 1
fi

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SHADER_DIR="$SCRIPT_DIR/resources/shaders"

mkdir -p "$SHADER_DIR"

echo "Compiling vertex shader..."
"$QSB" --glsl "100 es,120,150" --hlsl 50 --msl 12 \
    -o "$SHADER_DIR/texture.vert.qsb" \
    "$SHADER_DIR/texture.vert"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to compile vertex shader"
    exit 1
fi

echo "Compiling fragment shader..."
"$QSB" --glsl "100 es,120,150" --hlsl 50 --msl 12 \
    -o "$SHADER_DIR/texture.frag.qsb" \
    "$SHADER_DIR/texture.frag"

if [ $? -ne 0 ]; then
    echo "ERROR: Failed to compile fragment shader"
    exit 1
fi

echo ""
echo "Shaders compiled successfully!"
echo "Output files:"
echo "  - texture.vert.qsb"
echo "  - texture.frag.qsb"
echo ""