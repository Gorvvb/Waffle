//--------------------------
// - Waffle 2D -
// - Renderer2D Quad Shader
//--------------------------

#type vertex
#version 460 core

layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec4 a_Color;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in float a_TexIndex;
layout (location = 4) in vec2 a_TilingFactor;
layout (location = 5) in int a_EntityID;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

layout (location = 0) out vec4 v_Color;
layout (location = 1) out vec2 v_TexCoord;
layout (location = 2) out vec2 v_TilingFactor;
layout (location = 3) out flat float v_TexIndex;
layout (location = 4) out flat int v_EntityID;

void main()
{
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_TilingFactor = a_TilingFactor;
	v_TexIndex = a_TexIndex;
	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 460 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

layout (location = 0) in vec4 v_Color;
layout (location = 1) in vec2 v_TexCoord;
layout (location = 2) in vec2 v_TilingFactor;
layout (location = 3) in flat float v_TexIndex;
layout (location = 4) in flat int v_EntityID;

layout (binding = 0) uniform sampler2D u_Textures[32];

void main()
{
	vec4 texColor = v_Color;
	vec2 uv = v_TexCoord * v_TilingFactor;

	switch(int(v_TexIndex))
	{
		case  0: texColor *= texture(u_Textures[ 0], uv); break;
		case  1: texColor *= texture(u_Textures[ 1], uv); break;
		case  2: texColor *= texture(u_Textures[ 2], uv); break;
		case  3: texColor *= texture(u_Textures[ 3], uv); break;
		case  4: texColor *= texture(u_Textures[ 4], uv); break;
		case  5: texColor *= texture(u_Textures[ 5], uv); break;
		case  6: texColor *= texture(u_Textures[ 6], uv); break;
		case  7: texColor *= texture(u_Textures[ 7], uv); break;
		case  8: texColor *= texture(u_Textures[ 8], uv); break;
		case  9: texColor *= texture(u_Textures[ 9], uv); break;
		case 10: texColor *= texture(u_Textures[10], uv); break;
		case 11: texColor *= texture(u_Textures[11], uv); break;
		case 12: texColor *= texture(u_Textures[12], uv); break;
		case 13: texColor *= texture(u_Textures[13], uv); break;
		case 14: texColor *= texture(u_Textures[14], uv); break;
		case 15: texColor *= texture(u_Textures[15], uv); break;
		case 16: texColor *= texture(u_Textures[16], uv); break;
		case 17: texColor *= texture(u_Textures[17], uv); break;
		case 18: texColor *= texture(u_Textures[18], uv); break;
		case 19: texColor *= texture(u_Textures[19], uv); break;
		case 20: texColor *= texture(u_Textures[20], uv); break;
		case 21: texColor *= texture(u_Textures[21], uv); break;
		case 22: texColor *= texture(u_Textures[22], uv); break;
		case 23: texColor *= texture(u_Textures[23], uv); break;
		case 24: texColor *= texture(u_Textures[24], uv); break;
		case 25: texColor *= texture(u_Textures[25], uv); break;
		case 26: texColor *= texture(u_Textures[26], uv); break;
		case 27: texColor *= texture(u_Textures[27], uv); break;
		case 28: texColor *= texture(u_Textures[28], uv); break;
		case 29: texColor *= texture(u_Textures[29], uv); break;
		case 30: texColor *= texture(u_Textures[30], uv); break;
		case 31: texColor *= texture(u_Textures[31], uv); break;
	}

	if (texColor.a == 0.0)
		discard;

	o_Color = texColor;
	o_EntityID = v_EntityID;
}