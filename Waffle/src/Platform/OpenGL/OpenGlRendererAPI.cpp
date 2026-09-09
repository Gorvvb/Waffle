#include "wfpch.h"
#include "OpenGlRendererAPI.h"

#include <glad/glad.h>

#include <unordered_set>
#include <string>

#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

namespace Waffle {

	static void PrintStackBackTrace()
	{
		void* stack[32];
		USHORT frames = CaptureStackBackTrace(0, 32, stack, NULL);
		HANDLE process = GetCurrentProcess();

		static bool symbolsInitialized = false;
		if (!symbolsInitialized)
		{
			SymInitialize(process, NULL, TRUE);
			symbolsInitialized = true;
		}

		SYMBOL_INFO* symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
		if (!symbol) return;
		symbol->MaxNameLen = 255;
		symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

		WF_CORE_ERROR("----- OpenGL Error Callstack Trace -----");
		for (USHORT i = 0; i < frames; i++)
		{
			SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
			std::string name = symbol->Name;
			if (name.find("OpenGLMessageCallback") == std::string::npos &&
				name.find("glDebug") == std::string::npos)
			{
				WF_CORE_ERROR("  frame #{0}: {1}", i, name);
			}
		}
		WF_CORE_ERROR("----------------------------------------");
		free(symbol);
	}

	void OpenGLMessageCallback(
		unsigned source,
		unsigned type,
		unsigned id,
		unsigned severity,
		int length,
		const char* message,
		const void* userParam)
	{
		if (type == GL_DEBUG_TYPE_PERFORMANCE)
			return;

		GLint currentProgram = 0;
		GLint currentVAO = 0;
		GLint currentFBO = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
		glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &currentFBO);

		// The message is only guaranteed for `length` chars - don't rely on a
		// terminator the driver may not have written.
		std::string msg((length > 0) ? std::string(message, (size_t)length) : std::string(message));

		static std::unordered_set<std::string> s_LoggedMessages;
		std::string msgKey = msg + " [Prog:" + std::to_string(currentProgram) + " VAO:" + std::to_string(currentVAO) + " FBO:" + std::to_string(currentFBO) + "]";

		if (s_LoggedMessages.find(msgKey) != s_LoggedMessages.end())
			return;

		// Bound the dedup cache so long sessions don't grow it forever.
		if (s_LoggedMessages.size() >= 1024)
			s_LoggedMessages.clear();
		s_LoggedMessages.insert(msgKey);

		switch (severity)
		{
		case GL_DEBUG_SEVERITY_HIGH:
			WF_CORE_CRITICAL("[OpenGL Error] {0} (Program ID: {1}, VAO ID: {2}, FBO ID: {3})", msg, currentProgram, currentVAO, currentFBO);
			PrintStackBackTrace();
			return;
		case GL_DEBUG_SEVERITY_MEDIUM:
			WF_CORE_ERROR("[OpenGL Error] {0} (Program ID: {1}, VAO ID: {2}, FBO ID: {3})", msg, currentProgram, currentVAO, currentFBO);
			PrintStackBackTrace();
			return;
		case GL_DEBUG_SEVERITY_LOW:          WF_CORE_WARN("[OpenGL Warn] {0} (Program ID: {1}, VAO ID: {2}, FBO ID: {3})", msg, currentProgram, currentVAO, currentFBO); return;
		case GL_DEBUG_SEVERITY_NOTIFICATION: WF_CORE_TRACE("[OpenGL Info] {0} (Program ID: {1}, VAO ID: {2}, FBO ID: {3})", msg, currentProgram, currentVAO, currentFBO); return;
		}
	}


	void OpenGLRendererAPI::Init()
	{
		WF_PROFILE_FUNCTION();

		#ifdef WF_DEBUG
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(OpenGLMessageCallback, nullptr);

		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
		#endif

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable(GL_DEPTH_TEST);
		// LEQUAL, not the default GL_LESS: 2D sprites are routinely coplanar
		// (entities default to z=0 and are painter-sorted back-to-front), and
		// equal depths fail GL_LESS - the later sprite would be discarded
		// wherever it overlaps the earlier one.
		glDepthFunc(GL_LEQUAL);
		glEnable(GL_LINE_SMOOTH);

		// Core-profile GL guarantees only line width 1.0; clamp requests to
		// the device's supported range instead of logging GL_INVALID_VALUE.
		GLfloat lineWidthRange[2] = { 1.0f, 1.0f };
		glGetFloatv(GL_LINE_WIDTH_RANGE, lineWidthRange);
		m_MaxLineWidth = lineWidthRange[1];

		// Query the real sampler limit - GL guarantees only 16 texture image
		// units; batching must clamp to whatever this device actually has.
		GLint maxUnits = 32;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);
		if (maxUnits > 0)
			m_MaxTextureUnits = (uint32_t)maxUnits;
	}

	void OpenGLRendererAPI::SetViewPort(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
	{
		glViewport(x, y, width, height);
	}

	void OpenGLRendererAPI::SetClearColor(const glm::vec4& color)
	{
		glClearColor(color.r, color.g, color.b, color.a);
	}

	void OpenGLRendererAPI::Clear()
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void OpenGLRendererAPI::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t indexOffset)
	{
		vertexArray->Bind();
		uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
		glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, (const void*)(uintptr_t)(indexOffset * sizeof(uint32_t)));
	}

	void OpenGLRendererAPI::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t vertexOffset)
	{
		vertexArray->Bind();
		glDrawArrays(GL_LINES, vertexOffset, vertexCount);
	}

	void OpenGLRendererAPI::SetLineWidth(float width)
	{
		if (m_MaxLineWidth > 0.0f && width > m_MaxLineWidth)
			width = m_MaxLineWidth;
		if (width < 1.0f)
			width = 1.0f;
		glLineWidth(width);
	}
}