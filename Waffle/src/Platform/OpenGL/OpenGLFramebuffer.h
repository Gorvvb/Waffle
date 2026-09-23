#pragma once

#include "Waffle/Renderer/Framebuffer.h"

namespace Waffle {

	class OpenGLFrameBuffer : public Framebuffer
	{
	private:
		uint32_t m_RendererID = 0;
		FramebufferSpecification m_Specification;

		std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications;
		FramebufferTextureSpecification m_DepthAttachmentSpecification = FramebufferTextureFormat::None;

		std::vector<uint32_t> m_ColorAttachments;
		uint32_t m_DepthAttachment = 0;
	public:
		OpenGLFrameBuffer(const FramebufferSpecification& spec);
		virtual ~OpenGLFrameBuffer() override;

		// Owns raw GL handles - copying would double-delete them.
		OpenGLFrameBuffer(const OpenGLFrameBuffer&) = delete;
		OpenGLFrameBuffer& operator=(const OpenGLFrameBuffer&) = delete;

		void Invalidate();

		virtual void Bind() override;
		virtual void Unbind() override;

		virtual void Resize(uint32_t width, uint32_t height) override;
		virtual int ReadPixel(uint32_t attachmentIndex, int x, int y) override;

		virtual void ClearAttachment(uint32_t attachmentIndex, int value) override;

		virtual void* GetImGuiAttachmentId(uint32_t index = 0) const override
		{
			if (index >= m_ColorAttachments.size())
				return nullptr;
			return (void*)(uintptr_t)m_ColorAttachments[index];
		}

		// Raw GL texture name of a color attachment - only for use inside
		// Platform/OpenGL (sampling a render target in post-processing etc.).
		uint32_t GetColorAttachmentGLHandle(uint32_t index = 0) const
		{
			return (index < m_ColorAttachments.size()) ? m_ColorAttachments[index] : 0;
		}

		virtual const FramebufferSpecification& GetSpecification() const override { return m_Specification; }
	};
}