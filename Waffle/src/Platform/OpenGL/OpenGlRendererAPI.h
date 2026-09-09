#pragma once

#include "Waffle/Renderer/RendererAPI.h"

namespace Waffle {

	class OpenGLRendererAPI : public RendererAPI
	{
	public:
		virtual void Init() override;
		virtual void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

		virtual void SetClearColor(const glm::vec4& color) override;
		virtual void Clear() override;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0, uint32_t indexOffset = 0) override;

		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset = 0) override;

		virtual void SetLineWidth(float width) override;

		virtual uint32_t GetMaxTextureSlots() const override { return m_MaxTextureUnits; }

	private:
		uint32_t m_MaxTextureUnits = 32;
		float m_MaxLineWidth = 1.0f;
	};
}