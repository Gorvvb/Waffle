#pragma once

#include "Waffle/ImGui/ImGuiUtilities.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace Waffle::UI {

	void BeginPropertyGrid(uint32_t columns = 2, float columnWidth = 0.0f);
	void EndPropertyGrid();

	void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 0.0f);
	bool DrawColorEdit3(const std::string& label, glm::vec3& value, float columnWidth = 0.0f);
	bool DrawColorEdit4(const std::string& label, glm::vec4& value, float columnWidth = 0.0f);
	bool PropertyGridHeader(const std::string& label, bool defaultOpen = true);
	bool SearchWidget(std::string& searchString, const char* hint = "Search...", float width = 0.0f);

	bool PropertyCheckbox(const std::string& label, bool& value);
	bool PropertyFloat(const std::string& label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f);
	bool PropertyInt(const std::string& label, int& value, float speed = 1.0f, int min = 0, int max = 0);
	bool PropertyString(const std::string& label, std::string& value);
	bool PropertyDropdown(const std::string& label, const char* const* options, int optionCount, int* selectedOption);
	bool PropertyTexture(const std::string& label, uint32_t textureID, const std::string& path, std::string& outNewPath);

}
