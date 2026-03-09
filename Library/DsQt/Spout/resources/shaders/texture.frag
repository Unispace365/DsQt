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

    // Interleaved gradient noise dithering — adds ±0.5 LSB of noise to
    // eliminate visible banding when the swap chain is 8-bit (D3D/Vulkan).
    // Imperceptible on 10-bit targets. No color space side-effects.
    float dither = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    texColor.rgb += (dither - 0.5) / 255.0;

    fragColor = texColor * qt_Opacity;
}
