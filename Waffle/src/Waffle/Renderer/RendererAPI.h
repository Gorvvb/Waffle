#pragma once

#include <glm/glm.hpp>

#include "VertexArray.h"

namespace Waffle {
	
	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, OpenGL = 1, Vulkan = 2,
		};
	public:
		virtual ~RendererAPI() = default;
		
		virtual void Init() = 0;
		virtual void SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

		virtual void SetClearColor(const glm::vec4& color) = 0;
		virtual void Clear() = 0;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0, uint32_t indexOffset = 0) = 0;

		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset = 0) = 0;

		virtual void SetLineWidth(float width) = 0;

		// Device sampler-array limit (per fragment stage). Batching code must
		// not exceed this - a u_Textures[32] array fails to link on GPUs that
		// only expose 16 texture image units.
		virtual uint32_t GetMaxTextureSlots() const = 0;

		inline static API GetAPI() { return s_API; }
	private:
		static API s_API;
	};
}