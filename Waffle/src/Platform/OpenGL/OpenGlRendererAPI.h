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

		virtual uint32_t GetMaxTextureSlots() const override { return m_MaxTextureUnits; }

	private:
		uint32_t m_MaxTextureUnits = 32;
	};
}