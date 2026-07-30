//--------------------------
// - Waffle 2D -
// - Renderer2D Circle Shader
//--------------------------

#type vertex
#version 460 core

layout(location = 0) in vec3 a_WorldPosition;
layout(location = 1) in vec3 a_LocalPosition;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Thickness;
layout(location = 4) in float a_Fade;
layout(location = 5) in int a_EntityID;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

struct VertexOutput
{
	vec3 LocalPosition;
	vec4 Color;
	float Thickness;
	float Fade;
};

layout(location = 0) out VertexOutput Output;
layout(location = 4) out flat int v_EntityID;

void main()
{
	Output.LocalPosition = a_LocalPosition;
	Output.Color = a_Color;
	Output.Thickness = a_Thickness;
	Output.Fade = a_Fade;

	v_EntityID = a_EntityID;

	gl_Position = u_ViewProjection * vec4(a_WorldPosition, 1.0);
}

#type fragment
#version 460 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

struct VertexOutput
{
	vec3 LocalPosition;
	vec4 Color;
	float Thickness;
	float Fade;
};

layout(location = 0) in VertexOutput Input;
layout(location = 4) in flat int v_EntityID;

void main()
{
	// Calculate distance from center (0,0) to outer edge (1.0)
	float dist = length(Input.LocalPosition.xy);
	float fade = max(Input.Fade, 0.0001);
	float circle = 1.0 - smoothstep(1.0 - fade, 1.0, dist);

	if (Input.Thickness < 1.0)
	{
		float innerRadius = 1.0 - Input.Thickness;
		circle *= smoothstep(innerRadius - fade, innerRadius, dist);
	}

	if (circle == 0.0)
		discard;

	// Set output color
	o_Color = Input.Color;
	o_Color.a *= circle;

	o_EntityID = v_EntityID;
}