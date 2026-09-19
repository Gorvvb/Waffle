#pragma once

// ===========================================================================
// EditorTheme - shared design system for editor panels.
//
// The global ImGui style (ImGuiLayer::SetDarkThemeColors) already carries
// the base charcoal + waffle-amber palette; these helpers give panels a
// consistent vocabulary on top of it: section labels, accent/ghost buttons,
// toggle chips, info chips and empty states. Use them instead of ad-hoc
// colors so the whole editor keeps one voice.
// ===========================================================================

#include <imgui/imgui.h>
#include <string>

namespace Waffle::UI {

	namespace Theme {

		// Brand accent (matches ImGuiLayer theme + Hub).
		const ImVec4 Accent      = ImVec4(0.914f, 0.608f, 0.176f, 1.00f);
		const ImVec4 AccentHover = ImVec4(0.965f, 0.690f, 0.278f, 1.00f);
		const ImVec4 AccentWash  = ImVec4(0.914f, 0.608f, 0.176f, 0.14f);
		const ImVec4 OnAccent    = ImVec4(0.10f, 0.08f, 0.04f, 1.00f);

		// Semantic.
		const ImVec4 Danger      = ImVec4(0.72f, 0.24f, 0.24f, 1.00f);
		const ImVec4 DangerHover = ImVec4(0.88f, 0.32f, 0.32f, 1.00f);
		const ImVec4 Success     = ImVec4(0.32f, 0.78f, 0.44f, 1.00f);

		// Text tones.
		const ImVec4 Text      = ImVec4(0.91f, 0.92f, 0.94f, 1.00f);
		const ImVec4 TextDim   = ImVec4(0.56f, 0.59f, 0.64f, 1.00f);
		const ImVec4 TextFaint = ImVec4(0.40f, 0.43f, 0.48f, 1.00f);

		// Surfaces.
		const ImVec4 Panel     = ImVec4(0.13f, 0.14f, 0.165f, 1.00f);
		const ImVec4 PanelAlt  = ImVec4(0.18f, 0.195f, 0.225f, 1.00f);
		const ImVec4 Border    = ImVec4(0.21f, 0.23f, 0.27f, 0.60f);
		const ImVec4 BorderLit = ImVec4(0.28f, 0.30f, 0.36f, 0.90f);

		const float Rounding = 6.0f;

		inline ImU32 ToU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
	}

	// Solid brand-accent button (primary action).
	inline bool AccentButton(const char* label, const ImVec2& size = ImVec2(0, 0))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, Theme::Accent);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::AccentHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::AccentHover);
		ImGui::PushStyleColor(ImGuiCol_Text, Theme::OnAccent);
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(4);
		return clicked;
	}

	// Borderless button that lights up on hover (secondary action).
	inline bool GhostButton(const char* label, const ImVec2& size = ImVec2(0, 0))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::PanelAlt);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::Border);
		ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_Border, Theme::Border);
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar();
		return clicked;
	}

	inline bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, Theme::Danger);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::DangerHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, Theme::DangerHover);
		ImGui::PushStyleColor(ImGuiCol_Text, Theme::Text);
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleColor(4);
		return clicked;
	}

	// Pill-style toggle: accent-washed when on, quiet when off.
	inline bool ToggleChip(const char* label, bool active, const ImVec2& size = ImVec2(0, 0))
	{
		if (active)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, Theme::AccentWash);
			ImGui::PushStyleColor(ImGuiCol_Text, Theme::Accent);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::AccentWash);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Border, Theme::Accent);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Theme::PanelAlt);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Border, Theme::Border);
		}
		bool clicked = ImGui::Button(label, size);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
		return clicked;
	}

	// Small uppercase section heading with a thin rule underneath.
	inline void SectionLabel(const char* label, float afterGap = 6.0f)
	{
		ImGui::Dummy(ImVec2(0, 3.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, Theme::TextDim);
		ImGui::SetWindowFontScale(0.88f);
		char buf[128];
		snprintf(buf, sizeof(buf), "%s", label);
		for (char* c = buf; *c; ++c)
			if (*c >= 'a' && *c <= 'z') *c = *c - 'a' + 'A';
		ImGui::TextUnformatted(buf);
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PopStyleColor();

		ImVec2 p = ImGui::GetCursorScreenPos();
		ImVec2 w = ImGui::GetContentRegionAvail();
		ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + w.x, p.y + 1.0f),
			Theme::ToU32(Theme::Border), 0.0f);
		ImGui::Dummy(ImVec2(0, 3.0f + afterGap));
	}

	// Small rounded info chip (drawn, not interactive). Returns nothing;
	// call inside a window layout - consumes one line.
	inline void Chip(const char* text, const ImVec4& tint, const char* id = "##chip")
	{
		ImGui::PushStyleColor(ImGuiCol_Text, tint);
		ImGui::Text("%s", text);
		ImGui::PopStyleColor();
		(void)id;
	}

	// Centred two-line empty state.
	inline void EmptyState(const char* line1, const char* line2)
	{
		ImVec2 region = ImGui::GetContentRegionAvail();
		ImVec2 s1 = ImGui::CalcTextSize(line1), s2 = ImGui::CalcTextSize(line2);
		ImGui::Dummy(ImVec2(0, region.y * 0.30f));
		ImGui::SetCursorPosX((region.x - s1.x) * 0.5f + ImGui::GetCursorPosX());
		ImGui::SetWindowFontScale(1.18f);
		ImGui::TextColored(Theme::TextDim, "%s", line1);
		ImGui::SetWindowFontScale(1.0f);
		ImGui::SetCursorPosX((region.x - s2.x) * 0.5f + ImGui::GetCursorPosX());
		ImGui::TextColored(Theme::TextFaint, "%s", line2);
	}

} // namespace Waffle::UI
