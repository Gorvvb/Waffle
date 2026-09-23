#pragma once

#include "Waffle/RHI/GraphicsPipeline.h"
#include "Waffle/RHI/CommandBuffer.h"

#include <glad/glad.h>

namespace Waffle {

	class OpenGLFrameBuffer;

	// -------------------------------------------------------------------------
	// OpenGLGraphicsPipeline - program + fixed-function state, applied on bind.
	// -------------------------------------------------------------------------
	class OpenGLGraphicsPipeline : public GraphicsPipeline
	{
	public:
		OpenGLGraphicsPipeline(const Desc& desc)
			: GraphicsPipeline(desc) {}

		// Applies the shader program and render state (blend/depth).
		void Apply() const;
	};

	// -------------------------------------------------------------------------
	// OpenGLCommandBuffer - executes every call immediately.
	// -------------------------------------------------------------------------
	class OpenGLCommandBuffer : public CommandBuffer
	{
	public:
		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual void BeginRenderPass(const Ref<Framebuffer>& target, bool clear = true) override;
		virtual void BeginSwapchainPass(uint32_t width, uint32_t height) override;
		virtual void EndRenderPass() override;

		virtual void BindPipeline(const Ref<GraphicsPipeline>& pipeline) override;

		virtual void BindTextures(uint32_t firstSlot, const Ref<Texture2D>* textures, uint32_t count) override;
		virtual void BindFramebufferAttachment(uint32_t slot, const Ref<Framebuffer>& target, uint32_t attachmentIndex) override;

		virtual void UpdateUniformBuffer(const Ref<UniformBuffer>& ubo, const void* data, uint64_t size, uint64_t offset) override;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t indexOffset) override;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset) override;

		virtual void SetLineWidth(float width) override;
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool flipY = false) override;

	private:
		Ref<Framebuffer> m_ActiveFramebuffer;
	};

}
