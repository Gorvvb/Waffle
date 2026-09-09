#include "wfpch.h"
#include "PostProcessing.h"
#include "Waffle/Renderer/Framebuffer.h"
#include "Waffle/Renderer/Renderer.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <algorithm>

namespace Waffle {

	struct BloomMip
	{
		uint32_t FBO = 0;
		uint32_t Texture = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	struct PostProcessingData
	{
		uint32_t QuadVAO = 0;
		uint32_t QuadVBO = 0;

		uint32_t CompositeProgram = 0;
		uint32_t DownsampleProgram = 0;
		uint32_t UpsampleProgram = 0;

		Ref<Framebuffer> OutputFramebuffer;

		static const int BloomMipLevels = 5;
		BloomMip BloomMips[BloomMipLevels];
		uint32_t BloomWidth = 0;
		uint32_t BloomHeight = 0;
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

	static const char* g_DownsampleShaderSource = R"(
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SrcTexture;
uniform vec2 u_TexelSize;
uniform int u_MipLevel;
uniform float u_Threshold;

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
)";

	static const char* g_UpsampleShaderSource = R"(
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_SrcTexture;
uniform vec2 u_TexelSize;
uniform float u_FilterRadius;

vec3 UpsampleTent9(sampler2D tex, vec2 uv, vec2 texelSize, float radius)
{
    vec4 d = texelSize.xyxy * vec4(1.0, 1.0, -1.0, 0.0) * radius;

    vec3 s;
    s  = texture(tex, uv - d.xy).rgb;
    s += texture(tex, uv - d.wy).rgb * 2.0;
    s += texture(tex, uv - d.zy).rgb;

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
)";

	static const char* g_CompositeShaderSource = R"(
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec2 v_TexCoord;

uniform sampler2D u_ScreenTexture;
uniform sampler2D u_BloomTexture;

uniform bool u_EnablePostProcessing;

uniform bool u_EnableBloom;
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

	static uint32_t CreateShaderProgram(const char* vsSrc, const char* fsSrc)
	{
		uint32_t vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &vsSrc, nullptr);
		glCompileShader(vs);
		if (!CheckShaderCompileStatus(vs, "VERTEX"))
		{
			glDeleteShader(vs);
			return 0;
		}

		uint32_t fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &fsSrc, nullptr);
		glCompileShader(fs);
		if (!CheckShaderCompileStatus(fs, "FRAGMENT"))
		{
			glDeleteShader(vs);
			glDeleteShader(fs);
			return 0;
		}

		uint32_t program = glCreateProgram();
		glAttachShader(program, vs);
		glAttachShader(program, fs);
		glLinkProgram(program);

		glDeleteShader(vs);
		glDeleteShader(fs);

		if (!CheckProgramLinkStatus(program))
		{
			glDeleteProgram(program);
			return 0;
		}

		return program;
	}

	static void DestroyBloomMips()
	{
		for (int i = 0; i < PostProcessingData::BloomMipLevels; i++)
		{
			if (s_Data.BloomMips[i].FBO != 0)
			{
				glDeleteFramebuffers(1, &s_Data.BloomMips[i].FBO);
				glDeleteTextures(1, &s_Data.BloomMips[i].Texture);
				s_Data.BloomMips[i].FBO = 0;
				s_Data.BloomMips[i].Texture = 0;
			}
		}
	}

	// Memoized uniform locations. The post chain issued ~20 string-keyed
	// driver queries per frame; programs are immutable once linked. The
	// cache must be cleared in Shutdown (program ids can be recycled).
	static std::unordered_map<uint64_t, GLint> s_UniformLocations;

	static GLint GetUniformLocationCached(uint32_t program, const char* name)
	{
		uint64_t key = ((uint64_t)program << 32) ^ std::hash<std::string_view>()(name);
		auto it = s_UniformLocations.find(key);
		if (it != s_UniformLocations.end())
			return it->second;

		GLint location = glGetUniformLocation(program, name);
		s_UniformLocations[key] = location;
		return location;
	}

	static void RecreateBloomMips(uint32_t width, uint32_t height)
	{
		DestroyBloomMips();

		s_Data.BloomWidth = width;
		s_Data.BloomHeight = height;

		uint32_t mipWidth = width / 2;
		uint32_t mipHeight = height / 2;

		for (int i = 0; i < PostProcessingData::BloomMipLevels; i++)
		{
			if (mipWidth < 1) mipWidth = 1;
			if (mipHeight < 1) mipHeight = 1;

			s_Data.BloomMips[i].Width = mipWidth;
			s_Data.BloomMips[i].Height = mipHeight;

			glGenFramebuffers(1, &s_Data.BloomMips[i].FBO);
			glBindFramebuffer(GL_FRAMEBUFFER, s_Data.BloomMips[i].FBO);

			glGenTextures(1, &s_Data.BloomMips[i].Texture);
			glBindTexture(GL_TEXTURE_2D, s_Data.BloomMips[i].Texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, mipWidth, mipHeight, 0, GL_RGBA, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_Data.BloomMips[i].Texture, 0);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			mipWidth /= 2;
			mipHeight /= 2;
		}
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

		s_Data.CompositeProgram = CreateShaderProgram(g_VertexShaderSource, g_CompositeShaderSource);
		s_Data.DownsampleProgram = CreateShaderProgram(g_VertexShaderSource, g_DownsampleShaderSource);
		s_Data.UpsampleProgram = CreateShaderProgram(g_VertexShaderSource, g_UpsampleShaderSource);

		FramebufferSpecification spec;
		spec.Width = 1280;
		spec.Height = 720;
		spec.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };
		s_Data.OutputFramebuffer = Framebuffer::Create(spec);
	}

	void PostProcessing::Shutdown()
	{
		DestroyBloomMips();

		if (s_Data.QuadVAO != 0)
		{
			glDeleteVertexArrays(1, &s_Data.QuadVAO);
			glDeleteBuffers(1, &s_Data.QuadVBO);
			glDeleteProgram(s_Data.CompositeProgram);
			glDeleteProgram(s_Data.DownsampleProgram);
			glDeleteProgram(s_Data.UpsampleProgram);
			s_Data.QuadVAO = 0;
			// Program ids can be recycled by the driver after deletion.
			s_UniformLocations.clear();
		}
	}

	uint32_t PostProcessing::Process(uint32_t inputTextureID, uint32_t width, uint32_t height)
	{
		// The bloom/composite chain below drives raw OpenGL objects (FBOs,
		// programs, texture names). A Vulkan implementation has to live in
		// Platform/Vulkan; until then, never execute this on another backend.
		if (Renderer::GetAPI() != RendererAPI::API::OpenGL)
			return inputTextureID;

		if (width == 0 || height == 0 || inputTextureID == 0)
			return inputTextureID;

		if (s_Data.QuadVAO == 0)
			Init();

		if (s_Data.CompositeProgram == 0)
			return inputTextureID;

		const auto& spec = s_Data.OutputFramebuffer->GetSpecification();
		if (spec.Width != width || spec.Height != height)
		{
			s_Data.OutputFramebuffer->Resize(width, height);
		}

		if (s_Data.BloomWidth != width || s_Data.BloomHeight != height || s_Data.BloomMips[0].FBO == 0)
		{
			RecreateBloomMips(width, height);
		}

		GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
		GLboolean cullFace = glIsEnabled(GL_CULL_FACE);
		GLboolean blend = glIsEnabled(GL_BLEND);

		// The bloom upsample pass changes the blend *function* (not just the
		// enable bit) and the viewport/FBO bindings - save all of it so the
		// caller's state survives this pass exactly.
		GLint blendSrc = GL_ONE, blendDst = GL_ZERO;
		glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrc);
		glGetIntegerv(GL_BLEND_DST_RGB, &blendDst);
		GLint previousFBO = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFBO);
		GLint previousViewport[4] = { 0, 0, 1, 1 };
		glGetIntegerv(GL_VIEWPORT, previousViewport);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glBindVertexArray(s_Data.QuadVAO);

		// =========================================================================
		// 1. Bloom Multi-Pass Downsample & Upsample Chain
		// =========================================================================
		if (s_Settings.EnablePostProcessing && s_Settings.EnableBloom)
		{
			// ---- Downsample Chain ----
			glDisable(GL_BLEND);
			glUseProgram(s_Data.DownsampleProgram);

			uint32_t currentInputTex = inputTextureID;
			glm::vec2 currentSrcDim = glm::vec2((float)width, (float)height);

			for (int i = 0; i < PostProcessingData::BloomMipLevels; i++)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, s_Data.BloomMips[i].FBO);
				glViewport(0, 0, s_Data.BloomMips[i].Width, s_Data.BloomMips[i].Height);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, currentInputTex);
				glUniform1i(GetUniformLocationCached(s_Data.DownsampleProgram, "u_SrcTexture"), 0);

				glUniform2f(GetUniformLocationCached(s_Data.DownsampleProgram, "u_TexelSize"), 1.0f / currentSrcDim.x, 1.0f / currentSrcDim.y);
				glUniform1i(GetUniformLocationCached(s_Data.DownsampleProgram, "u_MipLevel"), i);
				glUniform1f(GetUniformLocationCached(s_Data.DownsampleProgram, "u_Threshold"), s_Settings.BloomThreshold);

				glDrawArrays(GL_TRIANGLES, 0, 6);

				currentInputTex = s_Data.BloomMips[i].Texture;
				currentSrcDim = glm::vec2((float)s_Data.BloomMips[i].Width, (float)s_Data.BloomMips[i].Height);
			}

			// ---- Upsample Chain (Additive Blending) ----
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glUseProgram(s_Data.UpsampleProgram);
			glUniform1f(GetUniformLocationCached(s_Data.UpsampleProgram, "u_FilterRadius"), 1.0f);

			for (int i = PostProcessingData::BloomMipLevels - 1; i > 0; i--)
			{
				const auto& nextMip = s_Data.BloomMips[i];
				const auto& targetMip = s_Data.BloomMips[i - 1];

				glBindFramebuffer(GL_FRAMEBUFFER, targetMip.FBO);
				glViewport(0, 0, targetMip.Width, targetMip.Height);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, nextMip.Texture);
				glUniform1i(GetUniformLocationCached(s_Data.UpsampleProgram, "u_SrcTexture"), 0);
				glUniform2f(GetUniformLocationCached(s_Data.UpsampleProgram, "u_TexelSize"), 1.0f / (float)nextMip.Width, 1.0f / (float)nextMip.Height);

				glDrawArrays(GL_TRIANGLES, 0, 6);
			}

			glDisable(GL_BLEND);
		}

		// =========================================================================
		// 2. Final Post-Processing Composite Pass
		// =========================================================================
		s_Data.OutputFramebuffer->Bind();
		glViewport(0, 0, width, height);
		glDisable(GL_BLEND);

		glUseProgram(s_Data.CompositeProgram);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, inputTextureID);
		glUniform1i(GetUniformLocationCached(s_Data.CompositeProgram, "u_ScreenTexture"), 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, s_Data.BloomMips[0].Texture);
		glUniform1i(GetUniformLocationCached(s_Data.CompositeProgram, "u_BloomTexture"), 1);

		glUniform1i(GetUniformLocationCached(s_Data.CompositeProgram, "u_EnablePostProcessing"), s_Settings.EnablePostProcessing ? 1 : 0);

		glUniform1i(GetUniformLocationCached(s_Data.CompositeProgram, "u_EnableBloom"), s_Settings.EnableBloom ? 1 : 0);
		glUniform1f(GetUniformLocationCached(s_Data.CompositeProgram, "u_BloomIntensity"), s_Settings.BloomIntensity);
		glUniform3fv(GetUniformLocationCached(s_Data.CompositeProgram, "u_BloomColor"), 1, glm::value_ptr(s_Settings.BloomColor));

		glUniform1i(GetUniformLocationCached(s_Data.CompositeProgram, "u_EnableVignette"), s_Settings.EnableVignette ? 1 : 0);
		glUniform1f(GetUniformLocationCached(s_Data.CompositeProgram, "u_VignetteIntensity"), s_Settings.VignetteIntensity);
		glUniform1f(GetUniformLocationCached(s_Data.CompositeProgram, "u_VignetteSmoothness"), s_Settings.VignetteSmoothness);
		glUniform3fv(GetUniformLocationCached(s_Data.CompositeProgram, "u_VignetteColor"), 1, glm::value_ptr(s_Settings.VignetteColor));

		glUniform1i(GetUniformLocationCached(s_Data.CompositeProgram, "u_EnableTonemapping"), s_Settings.EnableTonemapping ? 1 : 0);
		glUniform1f(GetUniformLocationCached(s_Data.CompositeProgram, "u_Exposure"), s_Settings.Exposure);
		glUniform1f(GetUniformLocationCached(s_Data.CompositeProgram, "u_Contrast"), s_Settings.Contrast);
		glUniform1f(GetUniformLocationCached(s_Data.CompositeProgram, "u_Saturation"), s_Settings.Saturation);
		glUniform3fv(GetUniformLocationCached(s_Data.CompositeProgram, "u_ColorGradingTint"), 1, glm::value_ptr(s_Settings.ColorGradingTint));

		glDrawArrays(GL_TRIANGLES, 0, 6);

		glBindVertexArray(0);
		glUseProgram(0);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previousFBO);
		glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
		glBlendFunc((GLenum)blendSrc, (GLenum)blendDst);
		if (depthTest) glEnable(GL_DEPTH_TEST);
		if (cullFace) glEnable(GL_CULL_FACE);
		if (blend) glEnable(GL_BLEND);

		return (uint32_t)s_Data.OutputFramebuffer->GetColorAttachmentRendererID(0);
	}

	void PostProcessing::PresentToScreen(uint32_t textureID, uint32_t width, uint32_t height)
	{
		if (Renderer::GetAPI() != RendererAPI::API::OpenGL)
			return;

		if (width == 0 || height == 0 || textureID == 0)
			return;

		if (s_Data.QuadVAO == 0)
			Init();

		if (s_Data.CompositeProgram == 0)
			return;

		GLint previousViewport[4] = { 0, 0, 1, 1 };
		glGetIntegerv(GL_VIEWPORT, previousViewport);
		// PresentToScreen may not be the caller's last GL operation - restore
		// the draw framebuffer it had, like Process does.
		GLint previousFBO = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFBO);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, width, height);

		GLboolean depthTest = glIsEnabled(GL_DEPTH_TEST);
		GLboolean cullFace = glIsEnabled(GL_CULL_FACE);
		GLboolean blend = glIsEnabled(GL_BLEND);

		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);

		glUseProgram(s_Data.CompositeProgram);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(GetUniformLocationCached(s_Data.CompositeProgram, "u_ScreenTexture"), 0);

		glUniform1i(GetUniformLocationCached(s_Data.CompositeProgram, "u_EnablePostProcessing"), 0);

		glBindVertexArray(s_Data.QuadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);

		glUseProgram(0);

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)previousFBO);
		glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

		if (depthTest) glEnable(GL_DEPTH_TEST);
		if (cullFace) glEnable(GL_CULL_FACE);
		if (blend) glEnable(GL_BLEND);
	}

}
