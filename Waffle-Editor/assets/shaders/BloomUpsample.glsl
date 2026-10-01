//--------------------------
// - Waffle -
// - Bloom Upsample
// 9-tap tent upsample, blended additively into the previous mip.
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
    float u_FilterRadius;
};

vec3 UpsampleTent9(sampler2D tex, vec2 uv, vec2 texelSize, float radius)
{
    vec4 d = texelSize.xyxy * vec4(1.0, 1.0, -1.0, 0.0) * radius;

    vec3 s;
    s  = texture(tex, uv - d.xy).rgb;
    s += texture(tex, uv - d.wy).rgb * 2.0;
    s += texture(tex, uv + d.zy).rgb;

    s += texture(tex, uv + d.zw).rgb * 2.0;
    s += texture(tex, uv       ).rgb * 4.0;
    s += texture(tex, uv + d.xw).rgb * 2.0;

    s += texture(tex, uv + d.zy).rgb;
    s += texture(tex, uv + d.wy).rgb * 2.0;
    s += texture(tex, uv + d.xy).rgb;

    return s * (1.0 / 16.0);
}

void main()
{
    vec3 color = UpsampleTent9(u_SrcTexture, v_TexCoord, u_TexelSize, u_FilterRadius);
    o_Color = vec4(color, 1.0);
}
