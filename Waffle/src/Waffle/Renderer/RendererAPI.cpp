#include "wfpch.h"
#include "RendererAPI.h"

namespace Waffle {

	RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;
	//RendererAPI::API RendererAPI::s_API = RendererAPI::API::Vulkan;
}