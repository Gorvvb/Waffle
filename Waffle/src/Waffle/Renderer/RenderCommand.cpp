#include "wfpch.h"
#include "RenderCommand.h"

#include "Platform/OpenGL/OpenGlRendererAPI.h"

namespace Waffle {

	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
}