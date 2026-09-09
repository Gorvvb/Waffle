#include "wfpch.h"
#include "Waffle/Core/Input.h"
#include "Waffle/Core/Application.h"
#include "Waffle/Core/KeyCodes.h"
#include "Waffle/Core/MouseCodes.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <unordered_map>
#include <map>

namespace Waffle {

	struct ActionBinding
	{
		std::vector<KeyCode> Keys;
		std::vector<MouseCode> MouseButtons;
	};

	struct AxisBinding
	{
		KeyCode PositiveKey = Key::None;
		KeyCode NegativeKey = Key::None;
	};

	static std::unordered_map<std::string, ActionBinding> s_ActionBindings;
	static std::unordered_map<std::string, AxisBinding> s_AxisBindings;

	static std::map<int, Controller> s_Controllers;
	static CursorMode s_CursorMode = CursorMode::Normal;

	// Per-frame keyboard/mouse snapshots for real Pressed/Released edges.
	static std::map<int, bool> s_KeyDownPrev, s_KeyDownCurr;
	static std::map<int, bool> s_MouseDownPrev, s_MouseDownCurr;

	static bool WasKeyDownPrev(KeyCode key)
	{
		auto it = s_KeyDownPrev.find((int)key);
		return it != s_KeyDownPrev.end() && it->second;
	}

	static bool IsKeyDownCurr(KeyCode key)
	{
		auto it = s_KeyDownCurr.find((int)key);
		return it != s_KeyDownCurr.end() && it->second;
	}

	static bool WasMouseDownPrev(MouseCode button)
	{
		auto it = s_MouseDownPrev.find((int)button);
		return it != s_MouseDownPrev.end() && it->second;
	}

	static bool IsMouseDownCurr(MouseCode button)
	{
		auto it = s_MouseDownCurr.find((int)button);
		return it != s_MouseDownCurr.end() && it->second;
	}

	void Input::Update()
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (!window) return;

		// Snapshot keyboard/mouse level state for edge queries
		// (IsKeyReleased / IsActionJustPressed previously had no real edges:
		// Released was just "!down", true every frame a key was simply up).
		s_KeyDownPrev = s_KeyDownCurr;
		s_KeyDownCurr.clear();
		for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
		{
			int state = glfwGetKey(window, key);
			if (state == GLFW_PRESS || state == GLFW_REPEAT)
				s_KeyDownCurr[key] = true;
		}

		s_MouseDownPrev = s_MouseDownCurr;
		s_MouseDownCurr.clear();
		for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
		{
			int state = glfwGetMouseButton(window, button);
			if (state == GLFW_PRESS)
				s_MouseDownCurr[button] = true;
		}

		for (int id = GLFW_JOYSTICK_1; id <= GLFW_JOYSTICK_LAST; ++id)
		{
			if (glfwJoystickPresent(id))
			{
				auto& controller = s_Controllers[id];
				controller.ID = id;
				const char* name = glfwGetJoystickName(id);
				controller.Name = name ? name : "Unknown";

				int axesCount = 0;
				const float* axes = glfwGetJoystickAxes(id, &axesCount);
				for (int i = 0; i < axesCount; ++i)
				{
					float val = axes[i];
					float dz = controller.DeadZones.count(i) ? controller.DeadZones[i] : 0.1f;
					if (std::abs(val) < dz) val = 0.0f;
					controller.AxisStates[i] = val;
				}

				int buttonsCount = 0;
				const unsigned char* buttons = glfwGetJoystickButtons(id, &buttonsCount);
				for (int i = 0; i < buttonsCount; ++i)
				{
					bool isDown = (buttons[i] == GLFW_PRESS);
					auto& bData = controller.ButtonStates[i];
					bData.Button = i;
					bData.OldState = bData.State;
					if (isDown)
					{
						bData.State = (bData.OldState == KeyState::Pressed || bData.OldState == KeyState::Held) ? KeyState::Held : KeyState::Pressed;
					}
					else
					{
						bData.State = (bData.OldState == KeyState::Pressed || bData.OldState == KeyState::Held) ? KeyState::Released : KeyState::None;
					}
					controller.ButtonDown[i] = isDown;
				}

				int hatsCount = 0;
				const unsigned char* hats = glfwGetJoystickHats(id, &hatsCount);
				for (int i = 0; i < hatsCount; ++i)
				{
					controller.HatStates[i] = hats[i];
				}
			}
			else
			{
				s_Controllers.erase(id);
			}
		}
	}

	bool Input::IsKeyDown(const KeyCode key)
	{
		auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (window)
		{
			auto state = glfwGetKey(window, static_cast<int32_t>(key));
			if (state == GLFW_PRESS || state == GLFW_REPEAT)
				return true;
		}

		if (ImGui::GetCurrentContext())
		{
			ImGuiKey imguiKey = ImGuiKey_None;
			if (key >= Key::A && key <= Key::Z)
				imguiKey = (ImGuiKey)(ImGuiKey_A + (key - Key::A));
			else if (key >= Key::D0 && key <= Key::D9)
				imguiKey = (ImGuiKey)(ImGuiKey_0 + (key - Key::D0));
			else if (key == Key::Space) imguiKey = ImGuiKey_Space;
			else if (key == Key::Left) imguiKey = ImGuiKey_LeftArrow;
			else if (key == Key::Right) imguiKey = ImGuiKey_RightArrow;
			else if (key == Key::Up) imguiKey = ImGuiKey_UpArrow;
			else if (key == Key::Down) imguiKey = ImGuiKey_DownArrow;
			else if (key == Key::Escape) imguiKey = ImGuiKey_Escape;
			else if (key == Key::Enter) imguiKey = ImGuiKey_Enter;

			if (imguiKey != ImGuiKey_None && ImGui::IsKeyDown(imguiKey))
				return true;
		}

		return false;
	}

	bool Input::IsKeyPressed(const KeyCode key)
	{
		// Level semantics (key is down) - kept deliberately: camera panning,
		// modifier checks and gameplay scripts all expect held behavior from
		// this name. True release edges live in IsKeyReleased; for a true
		// press edge use IsActionJustPressed with a bound action.
		return IsKeyDown(key);
	}

	bool Input::IsKeyHeld(const KeyCode key)
	{
		return IsKeyDown(key);
	}

	bool Input::IsKeyReleased(const KeyCode key)
	{
		// True release edge: was down last frame, up this frame.
		return WasKeyDownPrev(key) && !IsKeyDownCurr(key);
	}

	bool Input::IsMouseButtonDown(const MouseCode button)
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (window)
		{
			auto state = glfwGetMouseButton(window, static_cast<int32_t>(button));
			if (state == GLFW_PRESS)
				return true;
		}

		if (ImGui::GetCurrentContext())
		{
			if (ImGui::IsMouseDown((ImGuiMouseButton)button))
				return true;
		}

		return false;
	}

	bool Input::IsMouseButtonPressed(const MouseCode button)
	{
		// Level semantics, matching IsKeyPressed (see note there).
		return IsMouseButtonDown(button);
	}

	bool Input::IsMouseButtonHeld(const MouseCode button)
	{
		return IsMouseButtonDown(button);
	}

	bool Input::IsMouseButtonReleased(const MouseCode button)
	{
		// True release edge: was down last frame, up this frame.
		return WasMouseDownPrev(button) && !IsMouseDownCurr(button);
	}

	glm::vec2 Input::GetMousePosition()
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (!window) return { 0.0f, 0.0f };
		double xPos, yPos;
		glfwGetCursorPos(window, &xPos, &yPos);
		return { (float)xPos, (float)yPos };
	}

	float Input::GetMouseX()
	{
		return GetMousePosition().x;
	}

	float Input::GetMouseY()
	{
		return GetMousePosition().y;
	}

	void Input::SetCursorMode(CursorMode mode)
	{
		s_CursorMode = mode;
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (!window) return;

		int glfwMode = GLFW_CURSOR_NORMAL;
		switch (mode)
		{
		case CursorMode::Normal: glfwMode = GLFW_CURSOR_NORMAL; break;
		case CursorMode::Hidden: glfwMode = GLFW_CURSOR_HIDDEN; break;
		case CursorMode::Locked: glfwMode = GLFW_CURSOR_DISABLED; break;
		}
		glfwSetInputMode(window, GLFW_CURSOR, glfwMode);
	}

	CursorMode Input::GetCursorMode()
	{
		return s_CursorMode;
	}

	void Input::BindActionKey(const std::string& actionName, KeyCode key)
	{
		s_ActionBindings[actionName].Keys.push_back(key);
	}

	void Input::BindActionMouseButton(const std::string& actionName, MouseCode button)
	{
		s_ActionBindings[actionName].MouseButtons.push_back(button);
	}

	void Input::BindAxis(const std::string& axisName, KeyCode positiveKey, KeyCode negativeKey)
	{
		s_AxisBindings[axisName] = { positiveKey, negativeKey };
	}

	bool Input::IsActionPressed(const std::string& actionName)
	{
		auto it = s_ActionBindings.find(actionName);
		if (it == s_ActionBindings.end())
			return false;

		for (auto key : it->second.Keys)
		{
			if (IsKeyPressed(key))
				return true;
		}

		for (auto button : it->second.MouseButtons)
		{
			if (IsMouseButtonPressed(button))
				return true;
		}

		return false;
	}

	bool Input::IsActionJustPressed(const std::string& actionName)
	{
		// Real press edge (prev down, curr up... prev up && curr down) using
		// the per-frame snapshots - previously this was identical to
		// IsActionPressed and fired on every held frame.
		auto it = s_ActionBindings.find(actionName);
		if (it == s_ActionBindings.end())
			return false;

		for (auto key : it->second.Keys)
		{
			if (!WasKeyDownPrev(key) && IsKeyDownCurr(key))
				return true;
		}

		for (auto button : it->second.MouseButtons)
		{
			if (!WasMouseDownPrev(button) && IsMouseDownCurr(button))
				return true;
		}

		return false;
	}

	float Input::GetAxis(const std::string& axisName)
	{
		std::string name = axisName;
		for (auto& c : name) c = (char)tolower((unsigned char)c);

		// Look up with the lowercased name - the map may have been bound
		// with either casing.
		auto it = s_AxisBindings.find(name);
		if (it == s_AxisBindings.end())
			it = s_AxisBindings.find(axisName);
		if (it != s_AxisBindings.end())
		{
			float val = 0.0f;
			if (it->second.PositiveKey != Key::None && IsKeyPressed(it->second.PositiveKey))
				val += 1.0f;
			if (it->second.NegativeKey != Key::None && IsKeyPressed(it->second.NegativeKey))
				val -= 1.0f;
			return val;
		}

		float value = 0.0f;
		if (name == "horizontal")
		{
			if (IsKeyPressed(Key::A) || IsKeyPressed(Key::Left))
				value -= 1.0f;
			if (IsKeyPressed(Key::D) || IsKeyPressed(Key::Right))
				value += 1.0f;
		}
		else if (name == "vertical")
		{
			if (IsKeyPressed(Key::S) || IsKeyPressed(Key::Down))
				value -= 1.0f;
			if (IsKeyPressed(Key::W) || IsKeyPressed(Key::Up))
				value += 1.0f;
		}

		return value;
	}

	bool Input::IsControllerPresent(int id)
	{
		return s_Controllers.count(id) > 0;
	}

	std::vector<int> Input::GetConnectedControllerIDs()
	{
		std::vector<int> ids;
		for (const auto& kv : s_Controllers)
			ids.push_back(kv.first);
		return ids;
	}

	const Controller* Input::GetController(int id)
	{
		auto it = s_Controllers.find(id);
		return (it != s_Controllers.end()) ? &it->second : nullptr;
	}

	std::string Input::GetControllerName(int id)
	{
		const Controller* ctrl = GetController(id);
		// Returns by value: a view into the Controller would dangle as soon
		// as Update() erases the entry on disconnect.
		return ctrl ? ctrl->Name : std::string();
	}

	bool Input::IsControllerButtonPressed(int controllerID, int button)
	{
		const Controller* ctrl = GetController(controllerID);
		if (!ctrl) return false;
		auto it = ctrl->ButtonStates.find(button);
		return (it != ctrl->ButtonStates.end()) ? (it->second.State == KeyState::Pressed) : false;
	}

	bool Input::IsControllerButtonHeld(int controllerID, int button)
	{
		const Controller* ctrl = GetController(controllerID);
		if (!ctrl) return false;
		auto it = ctrl->ButtonStates.find(button);
		return (it != ctrl->ButtonStates.end()) ? (it->second.State == KeyState::Held) : false;
	}

	bool Input::IsControllerButtonDown(int controllerID, int button)
	{
		const Controller* ctrl = GetController(controllerID);
		if (!ctrl) return false;
		auto it = ctrl->ButtonDown.find(button);
		return (it != ctrl->ButtonDown.end()) ? it->second : false;
	}

	bool Input::IsControllerButtonReleased(int controllerID, int button)
	{
		const Controller* ctrl = GetController(controllerID);
		if (!ctrl) return false;
		auto it = ctrl->ButtonStates.find(button);
		return (it != ctrl->ButtonStates.end()) ? (it->second.State == KeyState::Released) : false;
	}

	float Input::GetControllerAxis(int controllerID, int axis)
	{
		const Controller* ctrl = GetController(controllerID);
		if (!ctrl) return 0.0f;
		auto it = ctrl->AxisStates.find(axis);
		return (it != ctrl->AxisStates.end()) ? it->second : 0.0f;
	}

	uint8_t Input::GetControllerHat(int controllerID, int hat)
	{
		const Controller* ctrl = GetController(controllerID);
		if (!ctrl) return 0;
		auto it = ctrl->HatStates.find(hat);
		return (it != ctrl->HatStates.end()) ? it->second : 0;
	}

	float Input::GetControllerDeadzone(int controllerID, int axis)
	{
		const Controller* ctrl = GetController(controllerID);
		if (!ctrl) return 0.1f;
		auto it = ctrl->DeadZones.find(axis);
		return (it != ctrl->DeadZones.end()) ? it->second : 0.1f;
	}

	void Input::SetControllerDeadzone(int controllerID, int axis, float deadzone)
	{
		// Only touch EXISTING controllers - operator[] would insert a ghost
		// entry that made IsControllerPresent(id) lie for an absent pad.
		auto it = s_Controllers.find(controllerID);
		if (it != s_Controllers.end())
			it->second.DeadZones[axis] = deadzone;
	}
}