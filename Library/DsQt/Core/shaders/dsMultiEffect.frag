#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float useOpacityMask;
    float invertOpacityMask;
    vec2 effectSize;
    vec4 edgeFade;       // top, right, bottom, left; logical pixels
    vec4 cornerRadii;    // top-left, top-right, bottom-right, bottom-left
} ubuf;

layout(binding = 1) uniform sampler2D source;
layout(binding = 2) uniform sampler2D alphaMask;

float edgeCoverage(float distanceFromEdge, float fadeDistance)
{
    if (fadeDistance <= 0.0)
        return 1.0;
    return clamp(distanceFromEdge / fadeDistance, 0.0, 1.0);
}

float roundedCoverage(vec2 point, vec2 size, vec4 radii)
{
    // Keep each arc in its own quadrant, including on very small items.
    vec2 halfSize = size * 0.5;
    radii = min(radii, vec4(min(halfSize.x, halfSize.y)));
    vec2 centered = point - halfSize;
    float radius = centered.x < 0.0
        ? (centered.y < 0.0 ? radii.x : radii.w)
        : (centered.y < 0.0 ? radii.y : radii.z);
    vec2 corner = abs(centered) - halfSize + vec2(radius);
    float distanceToEdge = length(max(corner, vec2(0.0)))
        + min(max(corner.x, corner.y), 0.0) - radius;
    // Derivatives keep the antialiasing transition near one screen pixel,
    // including at fractional scales and on high-DPI displays.
    float pixelWidth = max(fwidth(distanceToEdge), 0.0001);
    return 1.0 - smoothstep(-0.5 * pixelWidth, 0.5 * pixelWidth, distanceToEdge);
}

void main()
{
    vec4 pixel = texture(source, qt_TexCoord0);
    vec2 size = max(ubuf.effectSize, vec2(0.0001));
    vec2 point = qt_TexCoord0 * size;
    float coverage = 1.0;

    if (ubuf.useOpacityMask > 0.5) {
        float alpha = texture(alphaMask, qt_TexCoord0).a;
        coverage *= mix(alpha, 1.0 - alpha, ubuf.invertOpacityMask);
    }

    coverage *= edgeCoverage(point.y, ubuf.edgeFade.x);
    coverage *= edgeCoverage(size.x - point.x, ubuf.edgeFade.y);
    coverage *= edgeCoverage(size.y - point.y, ubuf.edgeFade.z);
    coverage *= edgeCoverage(point.x, ubuf.edgeFade.w);

    if (any(greaterThan(ubuf.cornerRadii, vec4(0.0))))
        coverage *= roundedCoverage(point, size, ubuf.cornerRadii);

    // Scale RGB as well as alpha: Qt Quick uses premultiplied textures.
    fragColor = pixel * coverage * ubuf.qt_Opacity;
}