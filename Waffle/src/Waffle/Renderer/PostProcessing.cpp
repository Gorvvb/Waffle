#include "wfpch.h"
#include "PostProcessing.h"
#include "Waffle/Renderer/Framebuffer.h"
#include "Waffle/Renderer/Renderer.h"
#include "Waffle/Renderer/UniformBuffer.h"
#include "Waffle/RHI/GraphicsPipeline.h"

#include <glm/gtc/type_ptr.hpp>

namespace Waffle {

	// Post shaders are plain .glsl files shared by both backends: Vulkan set/binding decorations (GL maps binding to the unit), per-pass params in a ring-sliced std140 UBO at binding 2 (see CommandBuffer::UpdateUniformBuffer).

	// std140 mirrors of the Params blocks; field order and padding must match the GLSL exactly (bool = 4 bytes, vec3 aligned to 16).
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

	// std140: a vec3 must start on a 16-byte boundary, but the scalar after it packs at 4 bytes - get it wrong and every later field shifts (once killed the vignette smoothness slider).
	struct CompositeParams
	{
		int32_t EnablePostProcessing; // 0
		int32_t EnableBloom;          // 4
		int32_t EnableVignette;       // 8
		int32_t EnableTonemapping;    // 12
		float BloomIntensity;         // 16
		float _pad0[3];               // 20-31 (vec3 alignment)
		glm::vec3 BloomColor;         // 32
		float VignetteIntensity;      // 44 (packs right after the vec3)
		float VignetteSmoothness;     // 48
		float _pad2[3];               // 52-63 (vec3 alignment)
		glm::vec3 VignetteColor;      // 64
		float Exposure;               // 76 (packs right after the vec3)
		float Contrast;               // 80
		float Saturation;             // 84
		float _pad4[2];               // 88-95 (vec3 alignment)
		glm::vec3 ColorGradingTint;   // 96
		float _pad5;                  // 108 (block padded to 16)
	};
	static_assert(sizeof(CompositeParams) == 112, "std140 mismatch");

	// Resources

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

		// Load through the central ShaderLibrary (path resolution + hot reload) like every other shader.
		s_Data.CompositeShader  = ShaderLibrary::Get().Load("assets/shaders/PostComposite.glsl");
		s_Data.DownsampleShader = ShaderLibrary::Get().Load("assets/shaders/BloomDownsample.glsl");
		s_Data.UpsampleShader   = ShaderLibrary::Get().Load("assets/shaders/BloomUpsample.glsl");

		// A failed load (missing/corrupt file) leaves the chain disabled instead of crashing later on null pipelines.
		if (!s_Data.CompositeShader || !s_Data.DownsampleShader || !s_Data.UpsampleShader)
		{
			WF_CORE_ERROR("PostProcessing: shader resources unavailable - post chain disabled.");
			return;
		}

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

	// Post chain

	Ref<Framebuffer> PostProcessing::Process(const Ref<Framebuffer>& src, uint32_t attachmentIndex, uint32_t width, uint32_t height, const PostProcessingSettings& settings)
	{
		WF_PROFILE_FUNCTION();

		if (!src || width == 0 || height == 0)
			return nullptr;

		// The composite shader would pass through unchanged anyway; skip the chain and save the blit.
		if (!settings.EnablePostProcessing)
			return nullptr;

		EnsureResources();
		EnsureSizes(width, height);

		if (!s_Data.CompositePipeline)
			return nullptr;

		CommandBuffer* cmd = Renderer::GetCommandBuffer();
		if (!cmd)
			return nullptr;

		// 1. Bloom multi-pass downsample & upsample chain
		if (settings.EnablePostProcessing && settings.EnableBloom)
		{
			DownsampleParams dsParams{};

			// Downsample chain (opaque, each mip cleared)
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

			// Upsample chain (ADDITIVE into the previous mip: no clear)
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

		// 2. Final composite pass
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

	void PostProcessing::ProcessAndPresent(const Ref<Framebuffer>& src, uint32_t attachmentIndex, uint32_t width, uint32_t height, const PostProcessingSettings& settings)
	{
		WF_PROFILE_FUNCTION();

		Ref<Framebuffer> processed = Process(src, attachmentIndex, width, height, settings);
		if (!processed)
		{
			// Post chain disabled - blit the source through a passthrough composite; the swapchain has no entity-ID attachment and the 2D shaders write one.
			EnsureResources();
			if (!s_Data.CompositePipeline)
				return;
			EnsureSizes(width, height);

			CommandBuffer* cmd = Renderer::GetCommandBuffer();

			CompositeParams params{};
			params.EnablePostProcessing = 0;
			cmd->UpdateUniformBuffer(s_Data.ParamsUniformBuffer, &params, sizeof(params), 0);

			cmd->BeginSwapchainPass(width, height);
			cmd->BindPipeline(s_Data.CompositePipeline);
			cmd->BindFramebufferAttachment(0, src, attachmentIndex);
			cmd->BindFramebufferAttachment(1, src, attachmentIndex);
			cmd->DrawIndexed(s_Data.QuadVertexArray, 6, 0);
			cmd->EndRenderPass();
			return;
		}

		CommandBuffer* cmd = Renderer::GetCommandBuffer();

		// Present: composite the processed image onto the swapchain with effects disabled (pure blit).
		CompositeParams params{};
		params.EnablePostProcessing = 0;
		cmd->UpdateUniformBuffer(s_Data.ParamsUniformBuffer, &params, sizeof(params), 0);

		cmd->BeginSwapchainPass(width, height);
		cmd->BindPipeline(s_Data.CompositePipeline);
		cmd->BindFramebufferAttachment(0, processed, 0);
		// The shader declares u_BloomTexture even when unused; give the descriptor a valid binding.
		cmd->BindFramebufferAttachment(1, processed, 0);
		cmd->DrawIndexed(s_Data.QuadVertexArray, 6, 0);
		cmd->EndRenderPass();
	}

}
