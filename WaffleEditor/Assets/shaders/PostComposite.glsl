//--------------------------
// - Waffle -
// - Post Processing Composite
// Applies bloom, vignette, tonemapping and color grading.
//--------------------------

#type vertex
#version 460 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}

#type fragment
#version 460 core

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_TexCoord;

layout (set = 1, binding = 0) uniform sampler2D u_ScreenTexture;
layout (set = 1, binding = 1) uniform sampler2D u_BloomTexture;

// Member ORDER must match CompositeParams in PostProcessing.cpp exactly:
// std140 assigns offsets positionally (4-byte scalars first, then vec3s at
// 16-byte alignment). The old interleaved order made every setting read
// from the wrong offset - which greyed out the whole frame and silently
// disabled the vignette.
layout(std140, binding = 2) uniform Params
{
    bool u_EnablePostProcessing;
    bool u_EnableBloom;
    bool u_EnableVignette;
    bool u_EnableTonemapping;

    float u_BloomIntensity;
    vec3 u_BloomColor;

    float u_VignetteIntensity;
    float u_VignetteSmoothness;
    vec3 u_VignetteColor;

    float u_Exposure;
    float u_Contrast;
    float u_Saturation;
    vec3 u_ColorGradingTint;
};

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec4 texColor = texture(u_ScreenTexture, v_TexCoord);
    vec3 color = texColor.rgb;

    if (!u_EnablePostProcessing)
    {
        o_Color = texColor;
        return;
    }

    if (u_EnableBloom)
    {
        vec3 bloom = texture(u_BloomTexture, v_TexCoord).rgb;
        color += bloom * u_BloomIntensity * u_BloomColor;
    }

    if (u_Exposure > 0.001)
        color *= u_Exposure;

    color *= u_ColorGradingTint;

    if (u_Contrast > 0.01 && abs(u_Contrast - 1.0) > 0.001)
    {
        color = (color - vec3(0.5)) * u_Contrast + vec3(0.5);
    }

    if (abs(u_Saturation - 1.0) > 0.001)
    {
        float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = mix(vec3(luminance), color, u_Saturation);
    }

    if (u_EnableTonemapping)
    {
        color = ACESFilm(color);
    }

    if (u_EnableVignette)
    {
        vec2 uv = v_TexCoord - vec2(0.5);
        float dist = length(uv);
        // Defined edges only (edge0 < edge1): the old call passed reversed
        // edges, which is undefined behavior per the GLSL spec.
        float vignette = 1.0 - smoothstep(u_VignetteSmoothness, u_VignetteSmoothness + u_VignetteIntensity, dist);
        color = mix(u_VignetteColor, color, vignette);
    }

    o_Color = vec4(color, texColor.a);
}
