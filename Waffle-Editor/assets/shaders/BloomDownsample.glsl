//--------------------------
// - Waffle -
// - Bloom Downsample
// 13-tap box downsample; mip 0 applies the soft-knee bloom prefilter.
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

layout (set = 1, binding = 0) uniform sampler2D u_SrcTexture;

layout(std140, binding = 2) uniform Params
{
    vec2 u_TexelSize;
    int u_MipLevel;
    float u_Threshold;
};

vec3 DownsampleBox13(sampler2D tex, vec2 uv, vec2 texelSize)
{
    vec3 a = texture(tex, uv + vec2(-2.0,  2.0) * texelSize).rgb;
    vec3 b = texture(tex, uv + vec2( 0.0,  2.0) * texelSize).rgb;
    vec3 c = texture(tex, uv + vec2( 2.0,  2.0) * texelSize).rgb;

    vec3 d = texture(tex, uv + vec2(-2.0,  0.0) * texelSize).rgb;
    vec3 e = texture(tex, uv).rgb;
    vec3 f = texture(tex, uv + vec2( 2.0,  0.0) * texelSize).rgb;

    vec3 g = texture(tex, uv + vec2(-2.0, -2.0) * texelSize).rgb;
    vec3 h = texture(tex, uv + vec2( 0.0, -2.0) * texelSize).rgb;
    vec3 i = texture(tex, uv + vec2( 2.0, -2.0) * texelSize).rgb;

    vec3 j = texture(tex, uv + vec2(-1.0,  1.0) * texelSize).rgb;
    vec3 k = texture(tex, uv + vec2( 1.0,  1.0) * texelSize).rgb;
    vec3 l = texture(tex, uv + vec2(-1.0, -1.0) * texelSize).rgb;
    vec3 m = texture(tex, uv + vec2( 1.0, -1.0) * texelSize).rgb;

    vec3 color = e * 0.125;
    color += (a + c + g + i) * 0.03125;
    color += (b + d + f + h) * 0.0625;
    color += (j + k + l + m) * 0.125;

    return color;
}

vec3 Prefilter(vec3 color, float threshold)
{
    float brightness = max(color.r, max(color.g, color.b));
    float knee = threshold * 0.5;
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);
    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 0.00001);
    return color * max(contribution, 0.0);
}

void main()
{
    vec3 color = DownsampleBox13(u_SrcTexture, v_TexCoord, u_TexelSize);
    if (u_MipLevel == 0)
    {
        color = Prefilter(color, u_Threshold);
    }
    o_Color = vec4(color, 1.0);
}
