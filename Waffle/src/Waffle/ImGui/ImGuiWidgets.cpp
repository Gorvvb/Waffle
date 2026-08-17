#include "wfpch.h"
#include "ImGuiWidgets.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Waffle::UI {

	static uint32_t s_GridCounter = 0;

	void BeginPropertyGrid(uint32_t columns, float columnWidth)
	{
		ImGui::PushID((int)s_GridCounter++);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
		ImGui::Columns(columns);

		float actualColWidth = columnWidth;
		if (actualColWidth <= 0.0f)
		{
			actualColWidth = ImGui::GetContentRegionAvail().x * 0.42f;
			if (actualColWidth < 140.0f) actualColWidth = 140.0f;
		}
		ImGui::SetColumnWidth(0, actualColWidth);
	}

	void EndPropertyGrid()
	{
		ImGui::Columns(1);
		ImGui::PopStyleVar(2);
		ImGui::PopID();
		s_GridCounter = 0;
	}

	void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue, float columnWidth)
	{
		ImFont* boldFont = ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : ImGui::GetFont();

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		float actualColWidth = columnWidth;
		if (actualColWidth <= 0.0f)
		{
			actualColWidth = ImGui::GetContentRegionAvail().x * 0.42f;
			if (actualColWidth < 140.0f) actualColWidth = 140.0f;
		}
		ImGui::SetColumnWidth(0, actualColWidth);
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2.0f, 0.0f });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 3.0f, 2.0f });

		float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImVec2 buttonSize = { 20.0f, lineHeight };

		// X
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.1f, 0.15f, 1.0f));
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// Y
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// Z
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.35f, 0.9f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.25f, 0.8f, 1.0f));
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopItemWidth();

		ImGui::PopStyleVar(2);
		ImGui::Columns(1);
		ImGui::PopID();
	}

	bool DrawColorEdit3(const std::string& label, glm::vec3& value, float columnWidth)
	{
		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		float actualColWidth = columnWidth;
		if (actualColWidth <= 0.0f)
		{
			actualColWidth = ImGui::GetContentRegionAvail().x * 0.42f;
			if (actualColWidth < 140.0f) actualColWidth = 140.0f;
		}
		ImGui::SetColumnWidth(0, actualColWidth);

		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		bool changed = ImGui::ColorEdit3("##Color", &value.x);

		ImGui::Columns(1);
		ImGui::PopID();
		return changed;
	}

	bool DrawColorEdit4(const std::string& label, glm::vec4& value, float columnWidth)
	{
		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		float actualColWidth = columnWidth;
		if (actualColWidth <= 0.0f)
		{
			actualColWidth = ImGui::GetContentRegionAvail().x * 0.42f;
			if (actualColWidth < 140.0f) actualColWidth = 140.0f;
		}
		ImGui::SetColumnWidth(0, actualColWidth);
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		bool changed = ImGui::ColorEdit4("##Color", &value.x);

		ImGui::Columns(1);
		ImGui::PopID();
		return changed;
	}

	bool PropertyGridHeader(const std::string& label, bool defaultOpen)
	{
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

		if (!defaultOpen)
			flags &= ~ImGuiTreeNodeFlags_DefaultOpen;

		return ImGui::TreeNodeEx((void*)label.c_str(), flags, "%s", label.c_str());
	}

	bool SearchWidget(std::string& searchString, const char* hint, float width)
	{
		bool modified = false;

		if (width > 0.0f)
			ImGui::PushItemWidth(width);

		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy_s(buffer, searchString.c_str(), sizeof(buffer));

		if (ImGui::InputTextWithHint("##Search", hint, buffer, sizeof(buffer)))
		{
			searchString = buffer;
			modified = true;
		}

		if (width > 0.0f)
			ImGui::PopItemWidth();

		return modified;
	}

	bool PropertyCheckbox(const std::string& label, bool& value)
	{
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();
		ImGui::PushID(label.c_str());
		bool changed = ImGui::Checkbox("##cb", &value);
		ImGui::PopID();
		ImGui::NextColumn();
		return changed;
	}

	bool PropertyFloat(const std::string& label, float& value, float speed, float min, float max)
	{
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();
		ImGui::PushID(label.c_str());
		bool changed = ImGui::DragFloat("##df", &value, speed, min, max, "%.2f");
		ImGui::PopID();
		ImGui::NextColumn();
		return changed;
	}

	bool PropertyInt(const std::string& label, int& value, float speed, int min, int max)
	{
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();
		ImGui::PushID(label.c_str());
		bool changed = ImGui::DragInt("##di", &value, speed, min, max);
		ImGui::PopID();
		ImGui::NextColumn();
		return changed;
	}

	bool PropertyString(const std::string& label, std::string& value)
	{
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();
		ImGui::PushID(label.c_str());

		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy_s(buffer, value.c_str(), sizeof(buffer));
		bool changed = false;
		if (ImGui::InputText("##st", buffer, sizeof(buffer)))
		{
			value = buffer;
			changed = true;
		}

		ImGui::PopID();
		ImGui::NextColumn();
		return changed;
	}

	bool PropertyDropdown(const std::string& label, const char* const* options, int optionCount, int* selectedOption)
	{
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();
		ImGui::PushID(label.c_str());

		bool changed = false;
		const char* current = (selectedOption && *selectedOption >= 0 && *selectedOption < optionCount) ? options[*selectedOption] : "Select";
		if (ImGui::BeginCombo("##dd", current))
		{
			for (int i = 0; i < optionCount; i++)
			{
				bool isSelected = (selectedOption && *selectedOption == i);
				if (ImGui::Selectable(options[i], isSelected))
				{
					if (selectedOption) *selectedOption = i;
					changed = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::PopID();
		ImGui::NextColumn();
		return changed;
	}

	bool PropertyTexture(const std::string& label, uint32_t textureID, const std::string& path, std::string& outNewPath)
	{
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();
		ImGui::PushID(label.c_str());

		bool changed = false;
		float buttonSize = 36.0f;
		if (textureID != 0)
		{
			if (ImGui::ImageButton("##texBtn", (ImTextureID)(uintptr_t)textureID, ImVec2(buttonSize, buttonSize), ImVec2(0, 1), ImVec2(1, 0)))
			{
			}
		}
		else
		{
			if (ImGui::Button("None", ImVec2(buttonSize, buttonSize)))
			{
			}
		}

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const wchar_t* itemPath = (const wchar_t*)payload->Data;
				std::filesystem::path p(itemPath);
				outNewPath = p.string();
				changed = true;
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine();
		std::string filename = path.empty() ? "[None]" : std::filesystem::path(path).filename().string();
		ImGui::TextUnformatted(filename.c_str());

		ImGui::PopID();
		ImGui::NextColumn();
		return changed;
	}

}
