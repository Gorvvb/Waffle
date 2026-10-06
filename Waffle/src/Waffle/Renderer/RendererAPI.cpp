#include "wfpch.h"
#include "RendererAPI.h"

namespace Waffle {

	namespace {
		// Initial backend
		RendererAPI::API SelectInitialAPI()
		{
			const char* env = std::getenv("WAFFLE_BACKEND");
			if (env && *env)
			{
				if (std::strcmp(env, "OpenGL") == 0 || _stricmp(env, "opengl") == 0)
					return RendererAPI::API::OpenGL;
				if (std::strcmp(env, "Vulkan") == 0 || _stricmp(env, "vulkan") == 0)
					return RendererAPI::API::Vulkan;
			}
			return RendererAPI::API::OpenGL;
		}
	}

	RendererAPI::API RendererAPI::s_API = SelectInitialAPI();

	void RendererAPI::SetAPI(API api) { s_API = api; }
}