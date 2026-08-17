#include "wfpch.h"
#include "PostProcessing.h"
#include "Waffle/Renderer/Framebuffer.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

namespace Waffle {

	struct PostProcessingData
	{
		uint32_t QuadVAO = 0;
		uint32_t QuadVBO = 0;
		uint32_t ShaderProgram = 0;
		Ref<Framebuffer> OutputFramebuffer;
	};

	static PostProcessingData s_Data;

	static const char* g_VertexShaderSource = R"(
#version 450 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";

	static const char* g_FragmentShaderSource = R"(
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_ScreenTexture;

uniform bool u_EnablePostProcessing;

uniform bool u_EnableBloom;
uniform float u_BloomThreshold;
uniform float u_BloomIntensity;
uniform vec3 u_BloomColor;

uniform bool u_EnableVignette;
uniform float u_VignetteIntensity;
uniform float u_VignetteSmoothness;
uniform vec3 u_VignetteColor;

uniform bool u_EnableTonemapping;
uniform float u_Exposure;
uniform float u_Contrast;
uniform float u_Saturation;
uniform vec3 u_ColorGradingTint;

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

    if (u_Exposure > 0.001)
        color *= u_Exposure;

    color *= u_ColorGradingTint;

    if (u_EnableBloom)
    {
        float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
        if (brightness > u_BloomThreshold)
        {
            vec3 brightColor = color * (brightness - u_BloomThreshold) * u_BloomIntensity * u_BloomColor;
            color += brightColor;
        }
    }

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
        float vignette = smoothstep(u_VignetteSmoothness, u_VignetteSmoothness - u_VignetteIntensity, dist);
        color = mix(u_VignetteColor, color, vignette);
    }

    o_Color = vec4(color, texColor.a);
}
)";

	static bool CheckShaderCompileStatus(uint32_t shader, const char* type)
	{
		int success;
		char infoLog[1024];
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
			WF_CORE_ERROR("PostProcessing Shader Compile Error ({0}): {1}", type, infoLog);
			return false;
		}
		return true;
	}

	static bool CheckProgramLinkStatus(uint32_t program)
	{
		int success;
		char infoLog[1024];
		glGetProgramiv(program, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(program, 1024, nullptr, infoLog);
			WF_CORE_ERROR("PostProcessing Program Link Error: {0}", infoLog);
			return false;
		}
		return true;
	}

	void PostProcessing::Init()
	{
		if (s_Data.QuadVAO != 0)
			return;

		float quadVertices[] = {
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f,

			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f,
			-1.0f,  1.0f,  0.0f, 1.0f
		};

		glGenVertexArrays(1, &s_Data.QuadVAO);
		glGenBuffers(1, &s_Data.QuadVBO);

		glBindVertexArray(s_Data.QuadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, s_Data.QuadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);

		uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &g_VertexShaderSource, nullptr);
		glCompileShader(vs);
		CheckShaderCompileStatus(vs, "VERTEX");

		uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &g_FragmentShaderSource, nullptr);
		glCompileShader(fs);
		CheckShaderCompileStatus(fs, "FRAGMENT");

		s_Data.ShaderProgram = glCreateProgram();
		glAttachShader(s_Data.ShaderProgram, vs);
		glAttachShader(s_Data.ShaderProgram, fs);
		glLinkProgram(s_Data.ShaderProgram);
		CheckProgramLinkStatus(s_Data.ShaderProgram);

		glDeleteShader(vs);
		glDeleteShader(fs);

		FramebufferSpecification spec;
		spec.Width = 1280;
		spec.Height = 720;
		spec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };
		s_Data.OutputFramebuffer = Framebuffer::Create(spec);
	}

	void PostProcessing::Shutdown()
	{
		if (s_Data.QuadVAO != 0)
		{
			glDeleteVertexArrays(1, &s_Data.QuadVAO);
			glDeleteBuffers(1, &s_Data.QuadVBO);
			glDeleteProgram(s_Data.ShaderProgram);
			s_Data.QuadVAO = 0;
		}
	}

	uint32_t PostProcessing::Process(uint32_t inputTextureID, uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || inputTextureID == 0)
			return inputTextureID;

		if (s_Data.QuadVAO == 0)
			Init();

		const auto& spec = s_Data.OutputFramebuffer->GetSpecification();
		if (spec.Width != width || spec.Height != height)
		{
			s_Data.OutputFramebuffer->Resize(width, height);
		}

		s_Data.OutputFramebuffer->Bind();
		glViewport(0, 0, width, height);

		GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
		GLboolean cullFace = glIsEnabled(GL_CULL_FACE);
		GLboolean blend = glIsEnabled(GL_BLEND);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);

		glUseProgram(s_Data.ShaderProgram);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, inputTextureID);
		glUniform1i(glGetUniformLocation(s_Data.ShaderProgram, "u_ScreenTexture"), 0);

		glUniform1i(glGetUniformLocation(s_Data.ShaderProgram, "u_EnablePostProcessing"), s_Settings.EnablePostProcessing ? 1 : 0);

		glUniform1i(glGetUniformLocation(s_Data.ShaderProgram, "u_EnableBloom"), s_Settings.EnableBloom ? 1 : 0);
		glUniform1f(glGetUniformLocation(s_Data.ShaderProgram, "u_BloomThreshold"), s_Settings.BloomThreshold);
		glUniform1f(glGetUniformLocation(s_Data.ShaderProgram, "u_BloomIntensity"), s_Settings.BloomIntensity);
		glUniform3fv(glGetUniformLocation(s_Data.ShaderProgram, "u_BloomColor"), 1, glm::value_ptr(s_Settings.BloomColor));

		glUniform1i(glGetUniformLocation(s_Data.ShaderProgram, "u_EnableVignette"), s_Settings.EnableVignette ? 1 : 0);
		glUniform1f(glGetUniformLocation(s_Data.ShaderProgram, "u_VignetteIntensity"), s_Settings.VignetteIntensity);
		glUniform1f(glGetUniformLocation(s_Data.ShaderProgram, "u_VignetteSmoothness"), s_Settings.VignetteSmoothness);
		glUniform3fv(glGetUniformLocation(s_Data.ShaderProgram, "u_VignetteColor"), 1, glm::value_ptr(s_Settings.VignetteColor));

		glUniform1i(glGetUniformLocation(s_Data.ShaderProgram, "u_EnableTonemapping"), s_Settings.EnableTonemapping ? 1 : 0);
		glUniform1f(glGetUniformLocation(s_Data.ShaderProgram, "u_Exposure"), s_Settings.Exposure);
		glUniform1f(glGetUniformLocation(s_Data.ShaderProgram, "u_Contrast"), s_Settings.Contrast);
		glUniform1f(glGetUniformLocation(s_Data.ShaderProgram, "u_Saturation"), s_Settings.Saturation);
		glUniform3fv(glGetUniformLocation(s_Data.ShaderProgram, "u_ColorGradingTint"), 1, glm::value_ptr(s_Settings.ColorGradingTint));

		glBindVertexArray(s_Data.QuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glUseProgram(0);

		if (depthTest) glEnable(GL_DEPTH_TEST);
		if (cullFace) glEnable(GL_CULL_FACE);
		if (blend) glEnable(GL_BLEND);

		s_Data.OutputFramebuffer->Unbind();

		return (uint32_t)s_Data.OutputFramebuffer->GetColorAttachmentRendererID(0);
	}

	void PostProcessing::PresentToScreen(uint32_t textureID, uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || textureID == 0)
			return;

		if (s_Data.QuadVAO == 0)
			Init();

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);

		GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
		GLboolean cullFace = glIsEnabled(GL_CULL_FACE);
		GLboolean blend = glIsEnabled(GL_BLEND);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);

		glUseProgram(s_Data.ShaderProgram);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(glGetUniformLocation(s_Data.ShaderProgram, "u_ScreenTexture"), 0);

		glUniform1i(glGetUniformLocation(s_Data.ShaderProgram, "u_EnablePostProcessing"), 0);

		glBindVertexArray(s_Data.QuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glUseProgram(0);

		if (depthTest) glEnable(GL_DEPTH_TEST);
		if (cullFace) glEnable(GL_CULL_FACE);
		if (blend) glEnable(GL_BLEND);
	}

}
