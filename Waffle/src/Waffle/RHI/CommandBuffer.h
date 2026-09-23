#pragma once

#include <glm/glm.hpp>

#include "Waffle/Core/Ref.h"
#include "Waffle/Renderer/Texture.h"
#include "Waffle/Renderer/Framebuffer.h"
#include "Waffle/Renderer/UniformBuffer.h"
#include "Waffle/Renderer/VertexArray.h"
#include "GraphicsPipeline.h"

namespace Waffle {

	// -------------------------------------------------------------------------
	// CommandBuffer
	// The single way the renderer (Renderer2D, PostProcessing, explicit
	// passes) talks to the backend.
	//   - OpenGL: every method executes immediately (GL's model preserved,
	//     record order == execution order).
	//   - Vulkan: methods record into the current frame's VkCommandBuffer,
	//     with draw-time state taken ONLY from what was explicitly bound on
	//     this object - no global "bound shader / VAO / texture slots"
	//     emulation in the context.
	// -------------------------------------------------------------------------
	class CommandBuffer : public RefCounted
	{
	public:
		virtual ~CommandBuffer() = default;

		// Factory for the main per-frame command buffer (owned by Renderer).
		static Ref<CommandBuffer> Create();

		// Clear color used by Clear() and clear-on-begin render passes.
		virtual void SetClearColor(const glm::vec4& color) = 0;

		// Clears the current render target. On Vulkan this also starts
		// swapchain rendering when no pass is active (legacy "clear before
		// anything is bound" behavior - explicit passes are preferred).
		virtual void Clear() = 0;

		// --- Render targets ---------------------------------------------------
		// Begins rendering into `target`. When `clear` (default) the color
		// attachments are cleared with the current clear color; pass false
		// for passes that must preserve or blend onto existing content
		// (e.g. additive bloom upsampling).
		virtual void BeginRenderPass(const Ref<Framebuffer>& target, bool clear = true) = 0;
		// Begins rendering to the present surface (swapchain / default FB).
		virtual void BeginSwapchainPass(uint32_t width, uint32_t height) = 0;
		virtual void EndRenderPass() = 0;

		// --- State ------------------------------------------------------------
		virtual void BindPipeline(const Ref<GraphicsPipeline>& pipeline) = 0;

		// Binds textures to consecutive sampler slots. Matches the 2D shader
		// convention: slot N == u_Textures[N]; on Vulkan a sampler declared
		// at binding B reads slot B (binding index == slot index).
		virtual void BindTextures(uint32_t firstSlot, const Ref<Texture2D>* textures, uint32_t count) = 0;

		// Binds a framebuffer color attachment as a sampled texture (post
		// chains). The attachment must be in a shader-readable layout, i.e.
		// its pass must have ended.
		virtual void BindFramebufferAttachment(uint32_t slot, const Ref<Framebuffer>& target, uint32_t attachmentIndex) = 0;

		// UBO update. GL: immediate. Vulkan: writes a fresh ring slice inside
		// the buffer and the descriptor set uses dynamic offsets, so each
		// recorded pass reads the data written for IT - with more than one
		// pass per frame, a single host-mapped UBO would otherwise be read
		// with the last pass's data (deferred execution).
		virtual void UpdateUniformBuffer(const Ref<UniformBuffer>& ubo, const void* data, uint64_t size, uint64_t offset) = 0;

		// --- Draws ------------------------------------------------------------
		// `vertexArray` is bound explicitly per draw - it is not implicit
		// state. On Vulkan it also determines the vertex-input layout of the
		// resolved pipeline.
		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t indexOffset) = 0;
		virtual void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset) = 0;

		virtual void SetLineWidth(float width) = 0;
		// flipY inverts the vertical axis (Vulkan's Y-down NDC); GL ignores it.
		virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool flipY = false) = 0;
	};

}
