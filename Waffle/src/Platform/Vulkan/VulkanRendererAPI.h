#pragma once

#include "Waffle/Renderer/RendererAPI.h"
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <Volk/volk.h>

namespace Waffle {

	class VulkanRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0, uint32_t indexOffset = 0) override;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset = 0) override;
		virtual void SetLineWidth(float width) override;

		virtual uint32_t GetMaxTextureSlots() const override;

	private:
		void BindPipelineAndDraw(const Ref<VertexArray>& vertexArray,
			VkPrimitiveTopology topology,
			uint32_t count,
			bool indexed,
			uint32_t offset = 0);
	};

} // namespace Waffle
