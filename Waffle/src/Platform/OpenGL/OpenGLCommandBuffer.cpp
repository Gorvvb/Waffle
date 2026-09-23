#include "wfpch.h"
#include "OpenGLCommandBuffer.h"
#include "OpenGlTexture.h"
#include "OpenGLFramebuffer.h"

namespace Waffle {

	// =========================================================================
	// OpenGLGraphicsPipeline
	// =========================================================================
	void OpenGLGraphicsPipeline::Apply() const
	{
		if (m_Desc.Shader)
			m_Desc.Shader->Bind(); // glUseProgram

		switch (m_Desc.Blending)
		{
		case BlendMode::SrcAlpha:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case BlendMode::Additive:
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			break;
		case BlendMode::Opaque:
		default:
			glDisable(GL_BLEND);
			break;
		}

		if (m_Desc.DepthTest)
		{
			glEnable(GL_DEPTH_TEST);
			// LEQUAL: 2D sprites are routinely coplanar (painter-sorted), the
			// GL default of GL_LESS would discard the later sprite.
			glDepthFunc(GL_LEQUAL);
			glDepthMask(m_Desc.DepthWrite ? GL_TRUE : GL_FALSE);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}
	}

	// =========================================================================
	// OpenGLCommandBuffer
	// =========================================================================
	void OpenGLCommandBuffer::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLCommandBuffer::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLCommandBuffer::BeginRenderPass(const Ref<Framebuffer>& target, bool clear)
	{
		WF_CORE_ASSERT(target, "BeginRenderPass requires a framebuffer - use BeginSwapchainPass for the default target!");
		target->Bind();
		if (clear)
			Clear();
		m_ActiveFramebuffer = target;
	}

	void OpenGLCommandBuffer::BeginSwapchainPass(uint32_t width, uint32_t height)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, (GLsizei)width, (GLsizei)height);
		Clear();
		m_ActiveFramebuffer = nullptr;
	}

	void OpenGLCommandBuffer::EndRenderPass()
	{
		if (m_ActiveFramebuffer)
		{
			m_ActiveFramebuffer->Unbind();
			m_ActiveFramebuffer = nullptr;
		}
	}

	void OpenGLCommandBuffer::BindPipeline(const Ref<GraphicsPipeline>& pipeline)
	{
		WF_CORE_ASSERT(pipeline, "BindPipeline - null pipeline!");
		static_cast<const OpenGLGraphicsPipeline*>(pipeline.get())->Apply();
	}

	void OpenGLCommandBuffer::BindTextures(uint32_t firstSlot, const Ref<Texture2D>* textures, uint32_t count)
	{
		for (uint32_t i = 0; i < count; i++)
		{
			if (textures[i])
				textures[i]->Bind(firstSlot + i);
		}
	}

	void OpenGLCommandBuffer::BindFramebufferAttachment(uint32_t slot, const Ref<Framebuffer>& target, uint32_t attachmentIndex)
	{
		auto* glfb = dynamic_cast<OpenGLFrameBuffer*>(target.get());
		WF_CORE_ASSERT(glfb, "BindFramebufferAttachment - not an OpenGL framebuffer!");
		glBindTextureUnit(slot, glfb->GetColorAttachmentGLHandle(attachmentIndex));
	}

	void OpenGLCommandBuffer::UpdateUniformBuffer(const Ref<UniformBuffer>& ubo, const void* data, uint64_t size, uint64_t offset)
	{
		ubo->SetData(data, (uint32_t)size, (uint32_t)offset);
	}

	void OpenGLCommandBuffer::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t indexOffset)
	{
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (const void*)(uintptr_t)(indexOffset * sizeof(uint32_t)));
	}

	void OpenGLCommandBuffer::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset)
	{
		vertexArray->Bind();
		glDrawArrays(GL_LINES, vertexOffset, vertexCount);
	}

	void OpenGLCommandBuffer::SetLineWidth(float width)
	{
		// Core GL guarantees only 1.0; clamp to the device range instead of
		// raising GL_INVALID_VALUE.
		static GLfloat range[2] = { 0.0f, 0.0f };
		if (range[1] == 0.0f)
			glGetFloatv(GL_LINE_WIDTH_RANGE, range);
		width = glm::clamp(width, range[0], range[1]);
		glLineWidth(width);
	}

	void OpenGLCommandBuffer::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool flipY)
	{
		(void)flipY; // GL viewport is already Y-up
		glViewport((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
	}

}
