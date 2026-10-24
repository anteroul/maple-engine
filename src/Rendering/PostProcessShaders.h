#ifndef MAPLEENGINE_POSTPROCESSSHADERS_H
#define MAPLEENGINE_POSTPROCESSSHADERS_H

/// GLSL sources for the bloom and glow passes.
///
/// They are embedded rather than loaded from src/Shaders/, so a game finds
/// them wherever it is run from. They are GLSL 1.20 because the engine draws
/// with the fixed-function pipeline: the glow pass has to read the matrices
/// and colour that glTranslatef and glColor3f set, which core GLSL cannot see.
namespace shaders {

/// Fullscreen pass. Drawn with a quad covering -1..1, so the texture
/// coordinate follows from the position.
inline constexpr const char* FULLSCREEN_VERTEX = R"(
#version 120
varying vec2 v_UV;

void main()
{
    v_UV = gl_Vertex.xy * 0.5 + 0.5;
    gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0);
}
)";

/// Draws an entity's own geometry into the glow buffer in its glow colour.
inline constexpr const char* GLOW_VERTEX = R"(
#version 120
varying vec3 v_Colour;

void main()
{
    v_Colour = gl_Color.rgb;
    gl_Position = ftransform();
}
)";

inline constexpr const char* GLOW_FRAGMENT = R"(
#version 120
varying vec3 v_Colour;
uniform vec3 u_Colour;
uniform float u_UseColour;
uniform float u_Intensity;

void main()
{
    gl_FragColor = vec4(mix(v_Colour, u_Colour, u_UseColour) * u_Intensity, 1.0);
}
)";

/// Scene and glow buffer down to the first, half size, bloom level. The
/// scene contributes only what is brighter than the threshold, with a soft
/// knee so that light does not switch on abruptly; glow contributes in full.
///
/// u_Threshold is (threshold, threshold - knee, 2 * knee, 0.25 / knee).
inline constexpr const char* PREFILTER_FRAGMENT = R"(
#version 120
varying vec2 v_UV;
uniform sampler2D u_Scene;
uniform sampler2D u_Glow;
uniform vec4 u_Threshold;

vec3 brightPart(vec3 colour)
{
    float brightness = max(colour.r, max(colour.g, colour.b));
    float soft = clamp(brightness - u_Threshold.y, 0.0, u_Threshold.z);
    soft = u_Threshold.w * soft * soft;
    float contribution = max(soft, brightness - u_Threshold.x) / max(brightness, 1e-4);
    return colour * contribution;
}

void main()
{
    // Each target pixel covers 2x2 source pixels, which one bilinear tap
    // at its centre averages.
    vec3 scene = brightPart(texture2D(u_Scene, v_UV).rgb);
    vec3 glow = texture2D(u_Glow, v_UV).rgb;
    gl_FragColor = vec4(scene + glow, 1.0);
}
)";

/// Halve a bloom level. Five bilinear taps cover a 4x4 block of the source,
/// weighted towards the middle, which keeps small bright shapes from
/// flickering as they move across pixel boundaries.
inline constexpr const char* DOWNSAMPLE_FRAGMENT = R"(
#version 120
varying vec2 v_UV;
uniform sampler2D u_Source;
uniform vec2 u_TexelSize;

void main()
{
    vec2 d = u_TexelSize;
    vec3 sum = texture2D(u_Source, v_UV).rgb * 4.0;
    sum += texture2D(u_Source, v_UV + vec2(-d.x, -d.y)).rgb;
    sum += texture2D(u_Source, v_UV + vec2( d.x, -d.y)).rgb;
    sum += texture2D(u_Source, v_UV + vec2(-d.x,  d.y)).rgb;
    sum += texture2D(u_Source, v_UV + vec2( d.x,  d.y)).rgb;
    gl_FragColor = vec4(sum / 8.0, 1.0);
}
)";

/// 3x3 tent filter, shared by the upsample and composite passes.
#define MAPLE_TENT_FILTER R"(
vec3 tent(sampler2D source, vec2 uv, vec2 d)
{
    vec3 sum = texture2D(source, uv).rgb * 4.0;
    sum += texture2D(source, uv + vec2(-d.x, 0.0)).rgb * 2.0;
    sum += texture2D(source, uv + vec2( d.x, 0.0)).rgb * 2.0;
    sum += texture2D(source, uv + vec2(0.0, -d.y)).rgb * 2.0;
    sum += texture2D(source, uv + vec2(0.0,  d.y)).rgb * 2.0;
    sum += texture2D(source, uv + vec2(-d.x, -d.y)).rgb;
    sum += texture2D(source, uv + vec2( d.x, -d.y)).rgb;
    sum += texture2D(source, uv + vec2(-d.x,  d.y)).rgb;
    sum += texture2D(source, uv + vec2( d.x,  d.y)).rgb;
    return sum / 16.0;
}
)"

/// Blur a smaller level up into the next larger one, which is added to
/// through blending. Run from the smallest level up, each level ends up
/// holding the light of every level below it, so the halo has both a tight
/// core and a wide falloff.
inline constexpr const char* UPSAMPLE_FRAGMENT = R"(
#version 120
varying vec2 v_UV;
uniform sampler2D u_Source;
uniform vec2 u_TexelSize;
uniform float u_Radius;
)" MAPLE_TENT_FILTER R"(
void main()
{
    gl_FragColor = vec4(tent(u_Source, v_UV, u_TexelSize * u_Radius), 1.0);
}
)";

/// Scene plus bloom, onto the window.
inline constexpr const char* COMPOSITE_FRAGMENT = R"(
#version 120
varying vec2 v_UV;
uniform sampler2D u_Scene;
uniform sampler2D u_Bloom;
uniform vec2 u_BloomTexelSize;
uniform float u_Intensity;
)" MAPLE_TENT_FILTER R"(
void main()
{
    vec3 scene = texture2D(u_Scene, v_UV).rgb;
    vec3 bloom = tent(u_Bloom, v_UV, u_BloomTexelSize);
    gl_FragColor = vec4(scene + bloom * u_Intensity, 1.0);
}
)";

#undef MAPLE_TENT_FILTER

} // namespace shaders

#endif //MAPLEENGINE_POSTPROCESSSHADERS_H
