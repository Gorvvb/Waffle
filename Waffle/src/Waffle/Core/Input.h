#pragma once

#include <glm/glm.hpp>
#include "Waffle/Core/KeyCodes.h"
#include "Waffle/Core/MouseCodes.h"

#include <map>
#include <string>
#include <vector>
#include <string_view>

namespace Waffle {

	enum class KeyState
	{
		None = 0,
		Pressed,
		Held,
		Released
	};

	enum class CursorMode
	{
		Normal = 0,
		Hidden,
		Locked
	};

	struct ControllerButtonData
	{
		int Button;
		KeyState State = KeyState::None;
		KeyState OldState = KeyState::None;
	};

	struct Controller
	{
		int ID;
		std::string Name;
		std::map<int, bool> ButtonDown;
		std::map<int, ControllerButtonData> ButtonStates;
		std::map<int, float> AxisStates;
		std::map<int, float> DeadZones;
		std::map<int, uint8_t> HatStates;
	};

	struct KeyData
	{
		KeyCode Key;
		KeyState State = KeyState::None;
		KeyState OldState = KeyState::None;
	};

	struct ButtonData
	{
		MouseCode Button;
		KeyState State = KeyState::None;
		KeyState OldState = KeyState::None;
	};

	class Input
	{
	public:
		static void Update();

		static bool IsKeyPressed(KeyCode key);
		static bool IsKeyHeld(KeyCode key);
		static bool IsKeyDown(KeyCode key);
		static bool IsKeyReleased(KeyCode key);

		static bool IsMouseButtonPressed(MouseCode button);
		static bool IsMouseButtonHeld(MouseCode button);
		static bool IsMouseButtonDown(MouseCode button);
		static bool IsMouseButtonReleased(MouseCode button);

		static glm::vec2 GetMousePosition();
		static float GetMouseX();
		static float GetMouseY();
		static float GetAxis(const std::string& axisName);

		static void SetCursorMode(CursorMode mode);
		static CursorMode GetCursorMode();

		// Action & Axis Mapping System
		static void BindActionKey(const std::string& actionName, KeyCode key);
		static void BindActionMouseButton(const std::string& actionName, MouseCode button);
		static void BindAxis(const std::string& axisName, KeyCode positiveKey, KeyCode negativeKey);

		static bool IsActionPressed(const std::string& actionName);
		static bool IsActionJustPressed(const std::string& actionName);

		// Controllers
		static bool IsControllerPresent(int id);
		static std::vector<int> GetConnectedControllerIDs();
		static const Controller* GetController(int id);
		static std::string GetControllerName(int id);

		static bool IsControllerButtonPressed(int controllerID, int button);
		static bool IsControllerButtonHeld(int controllerID, int button);
		static bool IsControllerButtonDown(int controllerID, int button);
		static bool IsControllerButtonReleased(int controllerID, int button);

		static float GetControllerAxis(int controllerID, int axis);
		static uint8_t GetControllerHat(int controllerID, int hat);

		static float GetControllerDeadzone(int controllerID, int axis);
		static void SetControllerDeadzone(int controllerID, int axis, float deadzone);
	};
}