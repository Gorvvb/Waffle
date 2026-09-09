#include "wfpch.h"
#include "OpenGLFramebuffer.h"

#include <glad/glad.h>

namespace Waffle {

	namespace Utils {

		static GLenum TextureTarget(bool multisampled)
		{
			return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		}

		static void CreateTextures(bool multisampled, uint32_t* outID, uint32_t count)
		{
			glCreateTextures(TextureTarget(multisampled), count, outID);
		}

		static void BindTexture(bool multisampled, uint32_t id)
		{
			glBindTexture(TextureTarget(multisampled), id);
		}

		static void AttachColorTexture(uint32_t id, int samples, GLenum internalFormat, GLenum format, uint32_t width, uint32_t height, int index)
		{
			bool multisampled = samples > 1;
			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
			}
			else
			{
				GLenum dataType = (format == GL_RED_INTEGER) ? GL_INT : GL_UNSIGNED_BYTE;
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, dataType, nullptr);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, TextureTarget(multisampled), id, 0);
		}

		static void AttachDepthTexture(uint32_t id, int samples, GLenum format, GLenum attachmentType, uint32_t width, uint32_t height)
		{
			bool multisampled = samples > 1;
			if (multisampled)
			{
				glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);
			}
			else
			{
				glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}

			glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, TextureTarget(multisampled), id, 0);
		}

		static bool IsDepthFormat(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case FramebufferTextureFormat::DEPTH24STENCIL8: return true;
			}
			return false;
		}

		static GLenum WaffleFramebufferTextureFormatToOpenGL(FramebufferTextureFormat format)
		{
			switch (format)
			{
				case Waffle::FramebufferTextureFormat::RGBA8:		return GL_RGBA8;
				case Waffle::FramebufferTextureFormat::RED_INTEGER: return GL_RED_INTEGER;
			}

			WF_CORE_ASSERT(false);
			return 0;
		}
	}

	OpenGLFrameBuffer::OpenGLFrameBuffer(const FramebufferSpecification& spec)
		: m_Specification(spec)
	{
		for (auto format : m_Specification.Attachments.Attachments)
		{
			if (!Utils::IsDepthFormat(format.TextureFormat))
				m_ColorAttachmentSpecifications.emplace_back(format);
			else
				m_DepthAttachmentSpecification = format;
		}

		Invalidate();
	}

	OpenGLFrameBuffer::~OpenGLFrameBuffer()
	{
		glDeleteFramebuffers(1, &m_RendererID);
		glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()), m_ColorAttachments.data());
		glDeleteTextures(1, &m_DepthAttachment);
	}

	void OpenGLFrameBuffer::Invalidate()
	{
		if (m_RendererID)
		{
			glDeleteFramebuffers(1, &m_RendererID);
			glDeleteTextures(static_cast<GLsizei>(m_ColorAttachments.size()), m_ColorAttachments.data());
			glDeleteTextures(1, &m_DepthAttachment);

			m_ColorAttachments.clear();
			m_DepthAttachment = 0;
		}

		glCreateFramebuffers(1, &m_RendererID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		

		bool multisample = m_Specification.Samples > 1;

		// Attachments
		if (m_ColorAttachmentSpecifications.size())
		{
			m_ColorAttachments.resize(m_ColorAttachmentSpecifications.size());
			Utils::CreateTextures(multisample, m_ColorAttachments.data(), static_cast<GLsizei>(m_ColorAttachments.size()));

			for (size_t i = 0; i < m_ColorAttachments.size(); i++)
			{
				Utils::BindTexture(multisample, m_ColorAttachments[i]);
				switch (m_ColorAttachmentSpecifications[i].TextureFormat)
				{
				case FramebufferTextureFormat::RGBA8:
					Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_RGBA8, GL_RGBA, m_Specification.Width, m_Specification.Height, (int)i);
					break;
				case FramebufferTextureFormat::RED_INTEGER:
					Utils::AttachColorTexture(m_ColorAttachments[i], m_Specification.Samples, GL_R32I, GL_RED_INTEGER, m_Specification.Width, m_Specification.Height, (int)i);
					break;
				}
			}
		}

		if (m_DepthAttachmentSpecification.TextureFormat != FramebufferTextureFormat::None)
		{
			Utils::CreateTextures(multisample, &m_DepthAttachment, 1);
			Utils::BindTexture(multisample, m_DepthAttachment);
			switch (m_DepthAttachmentSpecification.TextureFormat)
			{
				case FramebufferTextureFormat::DEPTH24STENCIL8:
					Utils::AttachDepthTexture(m_DepthAttachment, m_Specification.Samples, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.Width, m_Specification.Height);
					break;
			}
		}

		if (m_ColorAttachments.size() > 1)
		{
			WF_CORE_ASSERT(m_ColorAttachments.size() <= 4);
			GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
			glNamedFramebufferDrawBuffers(m_RendererID, static_cast<GLsizei>(m_ColorAttachments.size()), buffers);
		}
		else if (m_ColorAttachments.empty())
		{
			// Only depth-pass
			glNamedFramebufferDrawBuffer(m_RendererID, GL_NONE);
		}

		WF_CORE_ASSERT(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Framebuffer is incomplete!");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFrameBuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Specification.Width, m_Specification.Height);
	}

	void OpenGLFrameBuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void OpenGLFrameBuffer::Resize(uint32_t width, uint32_t height)
	{
		m_Specification.Width = width;
		m_Specification.Height = height;

		Invalidate();
	}

	int OpenGLFrameBuffer::ReadPixel(uint32_t attachmentIndex, int x, int y)
	{
		// Hard bounds check - the old assert compiles out in release and left
		// an unchecked vector index.
		if (attachmentIndex >= m_ColorAttachments.size())
			return -1;

		GLint previousFBO = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousFBO);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RendererID);
		glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex);

		int pixelData = -1;
		if (m_ColorAttachmentSpecifications[attachmentIndex].TextureFormat == FramebufferTextureFormat::RED_INTEGER)
		{
			glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
		}
		else
		{
			// Reading an RGBA8 attachment as RED_INTEGER/INT is an invalid
			// combination - read as RGBA/UNSIGNED_BYTE instead.
			unsigned char rgba[4] = { 0, 0, 0, 0 };
			glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
			pixelData = rgba[0];
		}

		// Read-buffer state is PER framebuffer object: the caller's
		// framebuffer was never touched (nothing to restore there), and this
		// FBO must be left on an attachment it actually has. Restoring the
		// DEFAULT framebuffer's read buffer (GL_BACK) onto this FBO raised
		// GL_INVALID_OPERATION ("the required buffer is missing").
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, previousFBO);
		return pixelData;
	}

	void OpenGLFrameBuffer::ClearAttachment(uint32_t attachmentIndex, int value)
	{
		if (attachmentIndex >= m_ColorAttachments.size())
			return;

		auto& spec = m_ColorAttachmentSpecifications[attachmentIndex];
		if (spec.TextureFormat == FramebufferTextureFormat::RED_INTEGER)
		{
			glClearNamedFramebufferiv(m_RendererID, GL_COLOR, (GLint)attachmentIndex, &value);
		}
		else
		{
			// glClearTexImage takes a pixel transfer format/type pair
			// (GL_RGBA/GL_UNSIGNED_BYTE), not the internal format, and needs a
			// full RGBA clear value.
			uint32_t rgba = 0;
			unsigned char* c = (unsigned char*)&rgba;
			c[0] = (unsigned char)(value & 0xFF);
			c[1] = (unsigned char)((value >> 8) & 0xFF);
			c[2] = (unsigned char)((value >> 16) & 0xFF);
			c[3] = (unsigned char)((value >> 24) & 0xFF);
			glClearTexImage(m_ColorAttachments[attachmentIndex], 0, GL_RGBA, GL_UNSIGNED_BYTE, &rgba);
		}
	}
}