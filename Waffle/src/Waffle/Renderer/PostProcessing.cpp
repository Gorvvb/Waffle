#include "wfpch.h"
#include "PostProcessing.h"
#include "Waffle/Renderer/Framebuffer.h"
#include "Waffle/Renderer/Renderer.h"
#include "Waffle/Renderer/UniformBuffer.h"
#include "Waffle/RHI/GraphicsPipeline.h"

#include <glm/gtc/type_ptr.hpp>

namespace Waffle {

	// =========================================================================
	// Shaders
	// One GLSL source per stage for BOTH backends: samplers carry Vulkan
	// set/binding decorations (GL ignores `set`, honours `binding` as the
	// texture unit), and per-pass parameters live in a std140 UBO at
	// binding 2 (Vulkan: set 0 / binding 2 - a ring-sliced UniformBuffer, so
	// every recorded pass keeps its own parameter set; see CommandBuffer::
	// UpdateUniformBuffer).
	// =========================================================================

	static const char* g_VertexShaderSource = R"(
#version 460 core

layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;

layout(location = 0) out vec2 v_TexCoord;

void main()
{
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";

	static const char* g_DownsampleShaderSource = R"(
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
)";

	static const char* g_UpsampleShaderSource = R"(
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
)";

	static const char* g_CompositeShaderSource = R"(
#version 460 core

layout(location = 0) out vec4 o_Color;

layout(location = 0) in vec2 v_TexCoord;

layout (set = 1, binding = 0) uniform sampler2D u_ScreenTexture;
layout (set = 1, binding = 1) uniform sampler2D u_BloomTexture;

layout(std140, binding = 2) uniform Params
{
    bool u_EnablePostProcessing;

    bool u_EnableBloom;
    float u_BloomIntensity;
    vec3 u_BloomColor;

    bool u_EnableVignette;
    float u_VignetteIntensity;
    float u_VignetteSmoothness;
    vec3 u_VignetteColor;

    bool u_EnableTonemapping;
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
)";

	// std140 mirrors of the Params blocks. Field order and padding must
	// match the GLSL exactly (bool == 4 bytes, vec3 aligned to 16).
	struct DownsampleParams
	{
		glm::vec2 TexelSize;
		int32_t MipLevel;
		float Threshold;
	};
	static_assert(sizeof(DownsampleParams) == 16, "std140 mismatch");

	struct UpsampleParams
	{
		glm::vec2 TexelSize;
		float FilterRadius;
		float _pad;
	};
	static_assert(sizeof(UpsampleParams) == 16, "std140 mismatch");

	struct CompositeParams
	{
		int32_t EnablePostProcessing; // 0
		int32_t EnableBloom;          // 4
		int32_t EnableVignette;       // 8
		int32_t EnableTonemapping;    // 12
		float BloomIntensity;         // 16
		float _pad0[3];
		glm::vec3 BloomColor;         // 32
		float _pad1;
		float VignetteIntensity;      // 48
		float VignetteSmoothness;     // 52
		float _pad2[2];
		glm::vec3 VignetteColor;      // 64
		float _pad3;
		float Exposure;               // 80
		float Contrast;               // 84
		float Saturation;             // 88
		float _pad4;
		glm::vec3 ColorGradingTint;   // 96
		float _pad5;
	};
	static_assert(sizeof(CompositeParams) == 112, "std140 mismatch");

	// =========================================================================
	// Resources
	// =========================================================================

	struct BloomMip
	{
		Ref<Framebuffer> Target;
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	struct PostProcessingData
	{
		Ref<VertexArray> QuadVertexArray;

		Ref<Shader> CompositeShader;
		Ref<Shader> DownsampleShader;
		Ref<Shader> UpsampleShader;

		Ref<GraphicsPipeline> CompositePipeline;
		Ref<GraphicsPipeline> DownsamplePipeline;
		Ref<GraphicsPipeline> UpsamplePipeline;

		// Per-pass parameters (binding 2) - ring-sliced on Vulkan.
		Ref<UniformBuffer> ParamsUniformBuffer;

		Ref<Framebuffer> OutputFramebuffer;

		static const int BloomMipLevels = 5;
		BloomMip BloomMips[BloomMipLevels];
		uint32_t BloomWidth = 0;
		uint32_t BloomHeight = 0;
	};

	static PostProcessingData s_Data;

	static void CreateBloomMips(uint32_t width, uint32_t height)
	{
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

			FramebufferSpecification spec;
			spec.Width = mipWidth;
			spec.Height = mipHeight;
			spec.Attachments = { FramebufferTextureFormat::RGBA16F };
			s_Data.BloomMips[i].Target = Framebuffer::Create(spec);

			mipWidth /= 2;
			mipHeight /= 2;
		}
	}

	void PostProcessing::EnsureResources()
	{
		if (s_Data.QuadVertexArray)
			return;

		// Fullscreen quad: two triangles with 0..1 UVs.
		float quadVertices[] = {
			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f, -1.0f,  1.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f,

			-1.0f, -1.0f,  0.0f, 0.0f,
			 1.0f,  1.0f,  1.0f, 1.0f,
			-1.0f,  1.0f,  0.0f, 1.0f
		};

		s_Data.QuadVertexArray = VertexArray::Create();
		Ref<VertexBuffer> quadVB = VertexBuffer::Create((float*)quadVertices, sizeof(quadVertices));
		quadVB->SetLayout({
			{ ShaderDataType::Float2, "a_Position" },
			{ ShaderDataType::Float2, "a_TexCoord" }
		});
		s_Data.QuadVertexArray->AddVertexBuffer(quadVB);

		uint32_t indices[6] = { 0, 1, 2, 3, 4, 5 };
		s_Data.QuadVertexArray->SetIndexBuffer(IndexBuffer::Create(indices, 6));

		// Shaders (compiled from memory on both backends).
		s_Data.CompositeShader  = Shader::Create("PostComposite",  g_VertexShaderSource, g_CompositeShaderSource);
		s_Data.DownsampleShader = Shader::Create("PostDownsample", g_VertexShaderSource, g_DownsampleShaderSource);
		s_Data.UpsampleShader   = Shader::Create("PostUpsample",   g_VertexShaderSource, g_UpsampleShaderSource);

		GraphicsPipeline::Desc desc;
		desc.DepthTest = false;   // fullscreen passes over colour-only targets
		desc.DepthWrite = false;

		desc.Blending = GraphicsPipeline::BlendMode::Opaque;
		desc.Shader = s_Data.CompositeShader;
		s_Data.CompositePipeline = GraphicsPipeline::Create(desc);

		desc.Shader = s_Data.DownsampleShader;
		s_Data.DownsamplePipeline = GraphicsPipeline::Create(desc);

		desc.Blending = GraphicsPipeline::BlendMode::Additive;
		desc.Shader = s_Data.UpsampleShader;
		s_Data.UpsamplePipeline = GraphicsPipeline::Create(desc);

		// Parameter UBO shared by all three shaders (binding 2).
		s_Data.ParamsUniformBuffer = UniformBuffer::Create(sizeof(CompositeParams), 2);

		FramebufferSpecification spec;
		spec.Width = 1280;
		spec.Height = 720;
		spec.Attachments = { FramebufferTextureFormat::RGBA8 };
		s_Data.OutputFramebuffer = Framebuffer::Create(spec);
	}

	void PostProcessing::EnsureSizes(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;

		const auto& spec = s_Data.OutputFramebuffer->GetSpecification();
		if (spec.Width != width || spec.Height != height)
			s_Data.OutputFramebuffer->Resize(width, height);

		if (s_Data.BloomWidth != width || s_Data.BloomHeight != height || !s_Data.BloomMips[0].Target)
			CreateBloomMips(width, height);
	}

	void PostProcessing::Init()
	{
		WF_PROFILE_FUNCTION();
		EnsureResources();
	}

	void PostProcessing::Shutdown()
	{
		s_Data.QuadVertexArray = nullptr;
		s_Data.CompositeShader = nullptr;
		s_Data.DownsampleShader = nullptr;
		s_Data.UpsampleShader = nullptr;
		s_Data.CompositePipeline = nullptr;
		s_Data.DownsamplePipeline = nullptr;
		s_Data.UpsamplePipeline = nullptr;
		s_Data.ParamsUniformBuffer = nullptr;
		s_Data.OutputFramebuffer = nullptr;
		for (auto& mip : s_Data.BloomMips)
			mip.Target = nullptr;
	}

	// =========================================================================
	// Post chain
	// =========================================================================

	Ref<Framebuffer> PostProcessing::Process(const Ref<Framebuffer>& src, uint32_t attachmentIndex, uint32_t width, uint32_t height)
	{
		WF_PROFILE_FUNCTION();

		if (!src || width == 0 || height == 0)
			return nullptr;

		EnsureResources();
		EnsureSizes(width, height);

		CommandBuffer* cmd = Renderer::GetCommandBuffer();
		if (!cmd)
			return nullptr;

		const auto& settings = s_Settings;

		// -----------------------------------------------------------------
		// 1. Bloom multi-pass downsample & upsample chain
		// -----------------------------------------------------------------
		if (settings.EnablePostProcessing && settings.EnableBloom)
		{
			DownsampleParams dsParams{};

			// ---- Downsample chain (opaque, each mip cleared) ----
			cmd->BindPipeline(s_Data.DownsamplePipeline);
			for (int i = 0; i < PostProcessingData::BloomMipLevels; i++)
			{
				glm::vec2 srcDim = (i == 0)
					? glm::vec2((float)width, (float)height)
					: glm::vec2((float)s_Data.BloomMips[i - 1].Width, (float)s_Data.BloomMips[i - 1].Height);

				dsParams.TexelSize = glm::vec2(1.0f) / srcDim;
				dsParams.MipLevel = i;
				dsParams.Threshold = settings.BloomThreshold;
				cmd->UpdateUniformBuffer(s_Data.ParamsUniformBuffer, &dsParams, sizeof(dsParams), 0);

				cmd->BeginRenderPass(s_Data.BloomMips[i].Target);

				if (i == 0)
					cmd->BindFramebufferAttachment(0, src, attachmentIndex);
				else
					cmd->BindFramebufferAttachment(0, s_Data.BloomMips[i - 1].Target, 0);

				cmd->DrawIndexed(s_Data.QuadVertexArray, 6, 0);
				cmd->EndRenderPass();
			}

			// ---- Upsample chain (ADDITIVE into the previous mip: no clear) ----
			UpsampleParams upParams{};
			upParams.FilterRadius = 1.0f;
			cmd->BindPipeline(s_Data.UpsamplePipeline);
			for (int i = PostProcessingData::BloomMipLevels - 1; i > 0; i--)
			{
				upParams.TexelSize = glm::vec2(1.0f) / glm::vec2((float)s_Data.BloomMips[i].Width, (float)s_Data.BloomMips[i].Height);
				cmd->UpdateUniformBuffer(s_Data.ParamsUniformBuffer, &upParams, sizeof(upParams), 0);

				cmd->BeginRenderPass(s_Data.BloomMips[i - 1].Target, /*clear=*/false);
				cmd->BindFramebufferAttachment(0, s_Data.BloomMips[i].Target, 0);
				cmd->DrawIndexed(s_Data.QuadVertexArray, 6, 0);
				cmd->EndRenderPass();
			}
		}

		// -----------------------------------------------------------------
		// 2. Final composite pass
		// -----------------------------------------------------------------
		CompositeParams params{};
		params.EnablePostProcessing = settings.EnablePostProcessing ? 1 : 0;
		params.EnableBloom = settings.EnableBloom ? 1 : 0;
		params.EnableVignette = settings.EnableVignette ? 1 : 0;
		params.EnableTonemapping = settings.EnableTonemapping ? 1 : 0;
		params.BloomIntensity = settings.BloomIntensity;
		params.BloomColor = settings.BloomColor;
		params.VignetteIntensity = settings.VignetteIntensity;
		params.VignetteSmoothness = settings.VignetteSmoothness;
		params.VignetteColor = settings.VignetteColor;
		params.Exposure = settings.Exposure;
		params.Contrast = settings.Contrast;
		params.Saturation = settings.Saturation;
		params.ColorGradingTint = settings.ColorGradingTint;
		cmd->UpdateUniformBuffer(s_Data.ParamsUniformBuffer, &params, sizeof(params), 0);

		cmd->BeginRenderPass(s_Data.OutputFramebuffer);
		cmd->BindPipeline(s_Data.CompositePipeline);
		cmd->BindFramebufferAttachment(0, src, attachmentIndex);

		if (settings.EnablePostProcessing && settings.EnableBloom)
			cmd->BindFramebufferAttachment(1, s_Data.BloomMips[0].Target, 0);

		cmd->DrawIndexed(s_Data.QuadVertexArray, 6, 0);
		cmd->EndRenderPass();

		return s_Data.OutputFramebuffer;
	}

	void PostProcessing::ProcessAndPresent(const Ref<Framebuffer>& src, uint32_t attachmentIndex, uint32_t width, uint32_t height)
	{
		WF_PROFILE_FUNCTION();

		Ref<Framebuffer> processed = Process(src, attachmentIndex, width, height);
		if (!processed)
			return;

		CommandBuffer* cmd = Renderer::GetCommandBuffer();

		// Present: composite the processed image straight onto the present
		// surface with post effects disabled (pure blit).
		CompositeParams params{};
		params.EnablePostProcessing = 0;
		cmd->UpdateUniformBuffer(s_Data.ParamsUniformBuffer, &params, sizeof(params), 0);

		cmd->BeginSwapchainPass(width, height);
		cmd->BindPipeline(s_Data.CompositePipeline);
		cmd->BindFramebufferAttachment(0, processed, 0);
		// The shader declares u_BloomTexture even when unused (disabled
		// path); give the descriptor a valid binding.
		cmd->BindFramebufferAttachment(1, processed, 0);
		cmd->DrawIndexed(s_Data.QuadVertexArray, 6, 0);
		cmd->EndRenderPass();
	}

}
