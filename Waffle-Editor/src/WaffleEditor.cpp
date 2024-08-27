#include <Waffle.h>
#include <Waffle/Core/EntryPoint.h>

#include "Platform/OpenGL/OpenGLShader.h"

#include "imgui/imgui.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "EditorLayer.h"

namespace Waffle {

	class WaffleEditor : public Application
	{
	public:
		WaffleEditor()
			: Application("Waffle Editor")
		{
			PushLayer(new EditorLayer());
		}

		~WaffleEditor()
		{
		}
	};

	Application* CreateApplication()
	{
		return new WaffleEditor();
	}
}
