#pragma once

#include <glm/glm.hpp>
#include "Waffle/Core/KeyCodes.h"
#include "Waffle/Core/MouseCodes.h"

namespace Waffle {

	class Input
	{
	public:
		static bool IsKeyPressed(KeyCode key);

		static bool IsMouseButtonPressed(MouseCode button);
		static glm::vec2 GetMousePosition();
		static float GetAxis(const std::string& axisName);
		static float GetMouseX();
		static float GetMouseY();

		// Action & Axis Mapping System
		static void BindActionKey(const std::string& actionName, KeyCode key);
		static void BindActionMouseButton(const std::string& actionName, MouseCode button);
		static void BindAxis(const std::string& axisName, KeyCode positiveKey, KeyCode negativeKey);

		static bool IsActionPressed(const std::string& actionName);
		static bool IsActionJustPressed(const std::string& actionName);
	};
}