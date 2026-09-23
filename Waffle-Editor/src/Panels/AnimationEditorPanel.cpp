#include "wfpch.h"
#include "AnimationEditorPanel.h"
#include "../EditorTheme.h"
#include "Waffle/Utils/PlatformUtils.h"

#include <imgui/imgui.h>
#include <glm/gtc/type_ptr.hpp>

namespace Waffle {

	extern std::filesystem::path g_AssetPath;

	AnimationEditorPanel::AnimationEditorPanel()
	{
	}

	void AnimationEditorPanel::SetContext(const Ref<Scene>& context)
	{
		m_Context = context;
	}

	void AnimationEditorPanel::SetSelectedEntity(Entity entity)
	{
		m_SelectedEntity = entity;
	}

	void AnimationEditorPanel::OnImGuiRender()
	{
		ImGui::Begin("Animation Editor", nullptr, ImGuiWindowFlags_NoScrollbar);

		if (!m_SelectedEntity)
		{
			UI::EmptyState("No entity selected",
				"Pick an entity in the Hierarchy to edit its animations");
			ImGui::End();
			return;
		}

		if (!m_SelectedEntity.HasComponent<AnimatorComponent>())
		{
			UI::EmptyState("No Animator on this entity",
				m_SelectedEntity.GetComponent<TagComponent>().Tag.c_str());
			ImGui::Spacing();
			float w = ImGui::CalcTextSize("Add Animator Component").x + 32.0f;
			ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - w) * 0.5f + ImGui::GetCursorPosX());
			if (UI::AccentButton("Add Animator Component"))
				m_SelectedEntity.AddComponent<AnimatorComponent>();
			ImGui::End();
			return;
		}

		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();

		// ── Header: entity name + transport controls ─────────────────────────
		{
			ImGui::BeginGroup();
			ImGui::SetWindowFontScale(1.12f);
			ImGui::TextColored(UI::Theme::Text, "%s", m_SelectedEntity.GetComponent<TagComponent>().Tag.c_str());
			ImGui::SetWindowFontScale(1.0f);
			ImGui::EndGroup();

			const float btnW = 62.0f;
			const float cluster = 3.0f * btnW + 2.0f * 6.0f;
			ImGui::SameLine(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - cluster);

			// Play / Pause toggles between states; Stop resets.
			if (UI::AccentButton(animator.IsPlaying ? " Pause " : " Play ", ImVec2(btnW, 30)))
				animator.IsPlaying = !animator.IsPlaying;
			ImGui::SameLine(0.0f, 6.0f);
			if (UI::GhostButton("Stop", ImVec2(btnW, 30)))
				animator.Stop();
			ImGui::SameLine(0.0f, 6.0f);
			if (animator.IsPlaying)
				ImGui::TextColored(UI::Theme::Success, "  playing");
			else
				ImGui::TextColored(UI::Theme::TextFaint, "  stopped");
		}

		ImGui::Spacing();

		// ── Clip bar: clip chips + new clip ──────────────────────────────────
		UI::SectionLabel("CLIPS");

		{
			// Wrap clip chips like a tag cloud.
			const float newW = 190.0f;
			for (auto& [name, clip] : animator.Clips)
			{
				bool active = (m_SelectedClipName == name);
				if (UI::ToggleChip(name.c_str(), active))
				{
					m_SelectedClipName = name;
					animator.Play(name);
				}
				ImGui::SameLine(0.0f, 6.0f);
				float rightEdge = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
				if (ImGui::GetCursorPosX() + newW > rightEdge)
					ImGui::NewLine(); // wrap chips that would run past the right edge
			}
			ImGui::SetNextItemWidth(newW - 44.0f);
			ImGui::InputTextWithHint("##NewClipInput", "new clip name", m_NewClipBuffer, sizeof(m_NewClipBuffer));
			ImGui::SameLine(0.0f, 6.0f);
			if (UI::GhostButton("+ Add", ImVec2(38.0f, 30.0f)))
			{
				std::string name(m_NewClipBuffer);
				if (!name.empty() && animator.Clips.find(name) == animator.Clips.end())
				{
					AnimationClip clip;
					clip.Name = name;
					animator.Clips[name] = clip;
					m_SelectedClipName = name;
					animator.CurrentClip = name;
				}
			}
		}

		if (m_SelectedClipName.empty() || animator.Clips.find(m_SelectedClipName) == animator.Clips.end())
		{
			ImGui::Spacing();
			UI::EmptyState("No clip selected", "Pick or create a clip above to edit its timeline");
			ImGui::End();
			return;
		}

		auto& clip = animator.Clips[m_SelectedClipName];

		// ── Clip settings row ────────────────────────────────────────────────
		{
			ImGui::SetNextItemWidth(170.0f);
			ImGui::DragFloat("FPS", &clip.FPS, 0.5f, 0.1f, 120.0f, "%.1f");
			ImGui::SameLine(0.0f, 10.0f);
			if (UI::ToggleChip("Loop", clip.Loop, ImVec2(70.0f, 28.0f)))
				clip.Loop = !clip.Loop;

			ImGui::SameLine(0.0f, 10.0f);
			if (UI::GhostButton("+ Frame", ImVec2(84.0f, 28.0f)))
			{
				clip.KeyframeImagePaths.push_back("");
				clip.RefreshSubTextures();
			}
			ImGui::SameLine(0.0f, 6.0f);
			ImGui::BeginDisabled(clip.KeyframeImagePaths.empty());
			if (UI::GhostButton("- Frame", ImVec2(84.0f, 28.0f)) && !clip.KeyframeImagePaths.empty())
			{
				clip.KeyframeImagePaths.pop_back();
				clip.RefreshSubTextures();
			}
			ImGui::EndDisabled();

			ImGui::SameLine(0.0f, 10.0f);
			ImGui::TextColored(UI::Theme::TextFaint, "%d frames  %.2fs",
				(int)clip.KeyframeImagePaths.size(),
				clip.KeyframeImagePaths.empty() ? 0.0f : (float)clip.KeyframeImagePaths.size() / std::max(0.1f, clip.FPS));
		}

		ImGui::Spacing();

		// ── Timeline strip ───────────────────────────────────────────────────
		UI::SectionLabel("TIMELINE   drag images from the Content Browser onto frames");

		const int frameCount = (int)clip.KeyframeImagePaths.size();
		const float thumbnailSize = 76.0f;

		if (frameCount == 0)
		{
			ImGui::BeginChild("##TimelineEmpty", ImVec2(0, 110), ImGuiChildFlags_Borders);
			UI::EmptyState("Timeline is empty", "Add frames with + Frame, then drop images onto them");
			ImGui::EndChild();
		}
		else
		{
			ImGui::BeginChild("##Timeline", ImVec2(0, 150), ImGuiChildFlags_Borders,
				ImGuiWindowFlags_HorizontalScrollbar);

			for (int i = 0; i < frameCount; i++)
			{
				if (i > 0)
					ImGui::SameLine(0.0f, 8.0f);

				ImGui::BeginGroup();
				ImGui::PushID(i);

				const bool isCurrentFrame = (animator.CurrentFrameIndex == i);
				const bool hasImage = (i < (int)clip.SubTextures.size() && clip.SubTextures[i] && clip.SubTextures[i]->GetTexture());

				ImVec2 frameSize = ImVec2(thumbnailSize, thumbnailSize);
				if (hasImage)
				{
					auto tex = clip.SubTextures[i]->GetTexture();
					const glm::vec2* uvs = clip.SubTextures[i]->GetTexCoords();

					// Compute aspect from the UV region size, not the full texture.
					float framePixelW = (uvs[1].x - uvs[0].x) * (float)tex->GetWidth();
					float framePixelH = std::abs((uvs[2].y - uvs[0].y) * (float)tex->GetHeight());
					if (framePixelW > 0.0f && framePixelH > 0.0f)
					{
						float aspect = framePixelW / framePixelH;
						if (aspect >= 1.0f) frameSize = ImVec2(thumbnailSize, thumbnailSize / aspect);
						else                frameSize = ImVec2(thumbnailSize * aspect, thumbnailSize);
					}
				}

				// Frame card: accent outline when current, quiet otherwise.
				ImGui::PushStyleColor(ImGuiCol_Button, isCurrentFrame
					? UI::Theme::AccentWash
					: ImVec4(0.16f, 0.17f, 0.20f, 0.85f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI::Theme::PanelAlt);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI::Theme::PanelAlt);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, isCurrentFrame ? 2.0f : 1.0f);
				ImGui::PushStyleColor(ImGuiCol_Border, isCurrentFrame ? UI::Theme::Accent : UI::Theme::Border);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

				if (hasImage)
				{
					auto tex = clip.SubTextures[i]->GetTexture();
					const glm::vec2* uvs = clip.SubTextures[i]->GetTexCoords();
					ImGui::ImageButton("##FrameKey", (ImTextureID)tex->GetImGuiTextureId(),
						frameSize, ImVec2(uvs[3].x, uvs[3].y), ImVec2(uvs[1].x, uvs[1].y),
						ImVec4(0, 0, 0, 0), UI::Theme::Accent);
				}
				else
				{
					ImGui::Button("Drop\nImage", frameSize);
				}

				ImGui::PopStyleVar(2);
				ImGui::PopStyleColor(4);

				// Receive Drag and Drop Image or Spritesheet Frame onto this specific keyframe slot
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
					{
						const wchar_t* pathStr = (const wchar_t*)payload->Data;
						std::filesystem::path path = std::filesystem::path(g_AssetPath) / pathStr;
						std::string ext = path.extension().string();
						for (auto& c : ext) c = (char)tolower(c);
						if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
						{
							clip.KeyframeImagePaths[i] = path.string();
							clip.RefreshSubTextures();
							WF_CORE_INFO("Assigned keyframe [{0}] texture: '{1}'", i, path.string());
						}
					}
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SPRITESHEET_FRAME_ITEM"))
					{
						const char* dataStr = (const char*)payload->Data;
						if (dataStr && payload->DataSize > 1)
						{
							clip.KeyframeImagePaths[i] = std::string(dataStr);
							clip.RefreshSubTextures();
							WF_CORE_INFO("Assigned keyframe [{0}] spritesheet frame: '{1}'", i, dataStr);
						}
					}
					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked())
					animator.CurrentFrameIndex = i;

				// Frame index badge (accent when current).
				char badge[16];
				snprintf(badge, sizeof(badge), "%d", i + 1);
				ImVec2 bs = ImGui::CalcTextSize(badge);
				float badgeW = bs.x + 12.0f;
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (frameSize.x - badgeW) * 0.5f);
				ImDrawList* dl = ImGui::GetWindowDrawList();
				ImVec2 bmin = ImGui::GetCursorScreenPos();
				ImVec2 bmax = ImVec2(bmin.x + badgeW, bmin.y + bs.y + 4.0f);
				dl->AddRectFilled(bmin, bmax,
					UI::Theme::ToU32(isCurrentFrame ? UI::Theme::Accent : UI::Theme::PanelAlt), 99.0f);
				dl->AddText(ImVec2(bmin.x + 6.0f, bmin.y + 2.0f),
					UI::Theme::ToU32(isCurrentFrame ? UI::Theme::OnAccent : UI::Theme::TextDim), badge);
				ImGui::Dummy(ImVec2(badgeW, bs.y + 4.0f));

				ImGui::PopID();
				ImGui::EndGroup();
			}

			ImGui::EndChild();
		}

		// ── Playhead ─────────────────────────────────────────────────────────
		ImGui::Spacing();
		if (!clip.SubTextures.empty())
		{
			ImGui::TextColored(UI::Theme::TextDim, "Playhead   ");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(220.0f);
			ImGui::SliderInt("##Playhead", &animator.CurrentFrameIndex, 0, (int)clip.SubTextures.size() - 1, "%d");
			ImGui::SameLine(0.0f, 8.0f);
			ImGui::TextColored(UI::Theme::TextFaint, "%d / %d",
				animator.CurrentFrameIndex + 1, std::max(1, (int)clip.SubTextures.size()));
		}

		ImGui::End();
	}

}
