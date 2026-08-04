#include "wfpch.h"
#include "AnimationEditorPanel.h"
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
		ImGui::Begin("Animation Editor");

		if (!m_SelectedEntity)
		{
			ImGui::TextDisabled("Select an entity in the Hierarchy to edit animations.");
			ImGui::End();
			return;
		}

		if (!m_SelectedEntity.HasComponent<AnimatorComponent>())
		{
			ImGui::Text("Entity '%s' does not have an Animator Component.", m_SelectedEntity.GetComponent<TagComponent>().Tag.c_str());
			if (ImGui::Button("Add Animator Component"))
			{
				m_SelectedEntity.AddComponent<AnimatorComponent>();
			}
			ImGui::End();
			return;
		}

		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();

		// Top Toolbar - Clip Selector & Playback Controls
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));

		if (ImGui::Button(animator.IsPlaying ? " Pause " : " Play "))
		{
			animator.IsPlaying = !animator.IsPlaying;
		}
		ImGui::SameLine();
		if (ImGui::Button(" Stop "))
		{
			animator.Stop();
		}

		ImGui::SameLine();
		ImGui::Text("Clip:");
		ImGui::SameLine();

		std::vector<std::string> clipNames;
		for (auto& [name, clip] : animator.Clips) clipNames.push_back(name);

		std::string currentLabel = m_SelectedClipName.empty() ? "(Select Clip)" : m_SelectedClipName;
		if (ImGui::BeginCombo("##ActiveClipCombo", currentLabel.c_str()))
		{
			for (auto& name : clipNames)
			{
				bool isSel = (m_SelectedClipName == name);
				if (ImGui::Selectable(name.c_str(), isSel))
				{
					m_SelectedClipName = name;
					animator.Play(name);
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(120);
		ImGui::InputText("##NewClipInput", m_NewClipBuffer, sizeof(m_NewClipBuffer));
		ImGui::SameLine();
		if (ImGui::Button("+ New Clip"))
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

		ImGui::PopStyleVar();
		ImGui::Separator();

		if (m_SelectedClipName.empty() || animator.Clips.find(m_SelectedClipName) == animator.Clips.end())
		{
			ImGui::TextDisabled("Select or create an animation clip above to edit its keyframe timeline.");
			ImGui::End();
			return;
		}

		auto& clip = animator.Clips[m_SelectedClipName];

		// Clip Settings Row
		ImGui::DragFloat("Playback Speed (FPS)", &clip.FPS, 0.5f, 0.1f, 120.0f);
		ImGui::SameLine();
		ImGui::Checkbox("Loop Clip", &clip.Loop);
		ImGui::SameLine();
		if (ImGui::Button("+ Add Keyframe Slot"))
		{
			clip.KeyframeImagePaths.push_back("");
			clip.RefreshSubTextures();
		}
		ImGui::SameLine();
		if (ImGui::Button("- Remove Keyframe") && !clip.KeyframeImagePaths.empty())
		{
			clip.KeyframeImagePaths.pop_back();
			clip.RefreshSubTextures();
		}

		ImGui::Separator();

		// Playhead scrub & status
		ImGui::Text("Playhead Frame: %d / %d", animator.CurrentFrameIndex, std::max(1, (int)clip.SubTextures.size()));
		if (!clip.SubTextures.empty())
		{
			ImGui::SliderInt("Playhead Position", &animator.CurrentFrameIndex, 0, (int)clip.SubTextures.size() - 1);
		}

		ImGui::Spacing();
		ImGui::Text("Horizontal Timeline (Drag PNG image files from Content Browser onto keyframe slots):");
		ImGui::Spacing();

		float thumbnailSize = 72.0f;
		int frameCount = (int)clip.KeyframeImagePaths.size();

		if (frameCount == 0)
		{
			ImGui::TextDisabled("No keyframes added yet. Click '+ Add Keyframe Slot' or drag image files here!");
		}

		ImGui::BeginChild("HorizontalTimelineRegion", ImVec2(0, 140.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

		for (int i = 0; i < frameCount; i++)
		{
			if (i > 0)
				ImGui::SameLine();

			ImGui::BeginGroup();
			ImGui::PushID(i);

			bool isCurrentFrame = (animator.CurrentFrameIndex == i);
			ImGui::PushStyleColor(ImGuiCol_Button, isCurrentFrame ? ImVec4(0.2f, 0.7f, 0.3f, 0.8f) : ImVec4(0.2f, 0.2f, 0.2f, 0.6f));

			ImVec2 frameSize = ImVec2(thumbnailSize, thumbnailSize);
			bool hasImage = (i < (int)clip.SubTextures.size() && clip.SubTextures[i] && clip.SubTextures[i]->GetTexture());
			if (hasImage)
			{
				auto tex = clip.SubTextures[i]->GetTexture();
				const glm::vec2* uvs = clip.SubTextures[i]->GetTexCoords();

				// Compute aspect from the UV region size, not the full texture.
				// uvs[0] = (min.x, min.y), uvs[2] = (max.x, max.y), so
				// height = max.y - min.y (may be negative due to OpenGL flip).
				float framePixelW = (uvs[1].x - uvs[0].x) * (float)tex->GetWidth();
				float framePixelH = std::abs((uvs[2].y - uvs[0].y) * (float)tex->GetHeight());
				if (framePixelW > 0.0f && framePixelH > 0.0f)
				{
					float aspect = framePixelW / framePixelH;
					if (aspect >= 1.0f)
						frameSize = ImVec2(thumbnailSize, thumbnailSize / aspect);
					else
						frameSize = ImVec2(thumbnailSize * aspect, thumbnailSize);
				}

				tex->SetFilter(TextureFilter::Nearest);
				ImGui::ImageButton("##FrameKey", (ImTextureID)(uintptr_t)tex->GetRendererID(),
					frameSize, ImVec2(uvs[3].x, uvs[3].y), ImVec2(uvs[1].x, uvs[1].y));
			}
			else
			{
				std::string slotLabel = "Drop Image\n[" + std::to_string(i) + "]";
				ImGui::Button(slotLabel.c_str(), ImVec2(thumbnailSize, thumbnailSize));
			}

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
						// Payload is "texturePath|minX,minY,maxX,maxY" - store as keyframe path.
						// RefreshSubTextures handles the pipe format directly.
						clip.KeyframeImagePaths[i] = std::string(dataStr);
						clip.RefreshSubTextures();
						WF_CORE_INFO("Assigned keyframe [{0}] spritesheet frame: '{1}'", i, dataStr);
					}
				}
				ImGui::EndDragDropTarget();
			}

			if (ImGui::IsItemClicked())
			{
				animator.CurrentFrameIndex = i;
			}

			ImGui::TextDisabled(" Frame %d", i);
			ImGui::PopStyleColor();
			ImGui::PopID();
			ImGui::EndGroup();
		}

		ImGui::EndChild();

		ImGui::End();
	}

}
