#include "wfpch.h"
#include "Waffle/Core/Input.h"
#include "Waffle/Core/Application.h"
#include "Waffle/Core/KeyCodes.h"
#include "Waffle/Core/MouseCodes.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

namespace Waffle {

	bool Input::IsKeyPressed(const KeyCode key)
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

	glm::vec2 Input::GetMousePosition()
	{
		auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		double xPos, yPos;
		glfwGetCursorPos(window, &xPos, &yPos);
		return { (float)xPos, (float)yPos };
	}

	bool Input::IsMouseButtonPressed(const MouseCode button)
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
		// Action check mapped to action bindings
		return IsActionPressed(actionName);
	}

	float Input::GetAxis(const std::string& axisName)
	{
		std::string name = axisName;
		for (auto& c : name) c = (char)tolower(c);

		// Check registered custom axis bindings first
		auto it = s_AxisBindings.find(axisName);
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

	float Input::GetMouseX()
	{
		return GetMousePosition().x;
	}

	float Input::GetMouseY()
	{
		return GetMousePosition().y;
	}
}