#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

layout(binding = 1) uniform sampler2D qt_Texture;

void main() {
    vec4 texColor = texture(qt_Texture, qt_TexCoord0);
    fragColor = texColor * qt_Opacity;
}
