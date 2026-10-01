#include "wfpch.h"
#include "ContentBrowserPanel.h"
#include "Waffle/Core/PlatformDetection.h"
#include "Waffle/Utils/PlatformUtils.h"
#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/Components.h"
#include "Waffle/Scene/SceneSerializer.h"

#include <imgui/imgui.h>
#include <yaml-cpp/yaml.h>
#include <fstream>
#include "Waffle/ImGui/ImGuiLayer.h"
#include <glm/glm.hpp>
#include <stb_image/stb_image.h>

namespace Waffle {

	std::filesystem::path g_AssetPath = "Assets";

	// Template for Content Browser > Create > Shader. Must keep the same
	// vertex inputs, Camera UBO and u_Textures sampler array as the built-in
	// 2DQuadShader.glsl - sprites feed it the same vertex stream and
	// Renderer2D binds textures to set 1 / binding 0 exactly as usual.
	static const char* k_NewShaderTemplate = R"(//--------------------------
// - Waffle -
// - Custom Sprite Shader
// Drop this .glsl onto a sprite's texture slot (or set it in the Sprite
// Renderer properties) to render that sprite with this shader.
//--------------------------

#type vertex
#version 460 core

// Keep this vertex body as-is: Renderer2D feeds this exact layout.
layout (location = 0) in vec3 a_Position;
layout (location = 1) in vec4 a_Color;
layout (location = 2) in vec2 a_TexCoord;
layout (location = 3) in float a_TexIndex;
layout (location = 4) in vec2 a_TilingFactor;
layout (location = 5) in int a_EntityID;

layout(std140, binding = 0) uniform Camera
{
	mat4 u_ViewProjection;
};

layout(location = 0) out vec4 v_Color;
layout(location = 1) out vec2 v_TexCoord;
layout(location = 2) out flat float v_TexIndex;
layout(location = 3) out vec2 v_TilingFactor;
layout(location = 4) out flat int v_EntityID;

void main()
{
	v_Color = a_Color;
	v_TexCoord = a_TexCoord;
	v_TexIndex = a_TexIndex;
	v_TilingFactor = a_TilingFactor;
	v_EntityID = a_EntityID;
	gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}

#type fragment
#version 460 core

layout(location = 0) out vec4 o_Color;
layout(location = 1) out int o_EntityID;

layout(location = 0) in vec4 v_Color;
layout(location = 1) in vec2 v_TexCoord;
layout(location = 2) in flat float v_TexIndex;
layout(location = 3) in vec2 v_TilingFactor;
layout(location = 4) in flat int v_EntityID;

#ifndef WF_MAX_TEXTURE_SLOTS
#define WF_MAX_TEXTURE_SLOTS 32
#endif

layout(set = 1, binding = 0) uniform sampler2D u_Textures[WF_MAX_TEXTURE_SLOTS];

vec4 SampleTexture(int index, vec2 uv)
{
	// Dynamically-uniform indexing workaround: a compile-time switch over
	// the sampler array (same approach as the built-in quad shader).
	switch (index)
	{
		case 0:  return texture(u_Textures[0],  uv);
		case 1:  return texture(u_Textures[1],  uv);
		case 2:  return texture(u_Textures[2],  uv);
		case 3:  return texture(u_Textures[3],  uv);
		case 4:  return texture(u_Textures[4],  uv);
		case 5:  return texture(u_Textures[5],  uv);
		case 6:  return texture(u_Textures[6],  uv);
		case 7:  return texture(u_Textures[7],  uv);
		case 8:  return texture(u_Textures[8],  uv);
		case 9:  return texture(u_Textures[9],  uv);
		case 10: return texture(u_Textures[10], uv);
		case 11: return texture(u_Textures[11], uv);
		case 12: return texture(u_Textures[12], uv);
		case 13: return texture(u_Textures[13], uv);
		case 14: return texture(u_Textures[14], uv);
		case 15: return texture(u_Textures[15], uv);
		case 16: return texture(u_Textures[16], uv);
		case 17: return texture(u_Textures[17], uv);
		case 18: return texture(u_Textures[18], uv);
		case 19: return texture(u_Textures[19], uv);
		case 20: return texture(u_Textures[20], uv);
		case 21: return texture(u_Textures[21], uv);
		case 22: return texture(u_Textures[22], uv);
		case 23: return texture(u_Textures[23], uv);
		case 24: return texture(u_Textures[24], uv);
		case 25: return texture(u_Textures[25], uv);
		case 26: return texture(u_Textures[26], uv);
		case 27: return texture(u_Textures[27], uv);
		case 28: return texture(u_Textures[28], uv);
		case 29: return texture(u_Textures[29], uv);
		case 30: return texture(u_Textures[30], uv);
		case 31: return texture(u_Textures[31], uv);
	}
	return vec4(1.0, 0.0, 1.0, 1.0); // magenta = bad texture slot
}

void main()
{
	vec4 texColor = SampleTexture(int(v_TexIndex), v_TexCoord * v_TilingFactor);

	// YOUR SHADER CODE HERE - this is the plain sprite tint:
	o_Color = texColor * v_Color;
	if (o_Color.a == 0.0)
		discard;

	o_EntityID = v_EntityID;
}
)";

	ContentBrowserPanel::ContentBrowserPanel()
		: m_CurrentDirectory(g_AssetPath)
	{
		m_DirectoryIcon = Texture2D::Create("Resources/Icons/ContentBrowser/DirectoryIcon.png", TextureFilter::Linear);
		m_FileIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FileIcon.png", TextureFilter::Linear);
	}

	void ContentBrowserPanel::SetAssetDirectory(const std::filesystem::path& path)
	{
		g_AssetPath = path;
		m_CurrentDirectory = g_AssetPath;
		// Thumbnails are keyed by absolute path - drop them on project switch
		// or stale cross-project entries linger forever.
		m_TextureCache.clear();
	}

	// 1x1 solid-color texture used as the thumbnail for prefabs whose
	// renderer only carries a color. Cached per quantized RGBA so different
	// colors don't thrash one texture, and identical ones share it.
	Ref<Texture2D> ContentBrowserPanel::GetColorSwatch(const glm::vec4& color)
	{
		char key[32];
		snprintf(key, sizeof(key), "color:%02X%02X%02X%02X",
			(int)(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f),
			(int)(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f),
			(int)(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f),
			(int)(glm::clamp(color.a, 0.0f, 1.0f) * 255.0f));

		auto it = m_TextureCache.find(key);
		if (it != m_TextureCache.end())
			return it->second;

		Ref<Texture2D> tex = Texture2D::Create(1, 1);
		if (!tex)
			return nullptr;

		uint32_t rgba =
			((uint32_t)(glm::clamp(color.a, 0.0f, 1.0f) * 255.0f) << 24) |
			((uint32_t)(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f) << 16) |
			((uint32_t)(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f) << 8)  |
			(uint32_t)(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
		tex->SetData(&rgba, sizeof(uint32_t));

		m_TextureCache[key] = tex;
		return tex;
	}

	// Prefab thumbnail = sprite texture tinted with the renderer color, the
	// same multiply the quad shader does (texColor * Color). Loaded via stb,
	// downsampled to <=128px, uploaded as a static texture. Cached per
	// (path, color) pair so nothing re-decodes per frame; failures fall back
	// to the untinted GPU texture (or the file icon) which is also cached.
	Ref<Texture2D> ContentBrowserPanel::GetTintedThumbnail(const std::filesystem::path& texturePath, const glm::vec4& color)
	{
		auto channelHex = [](float v) -> int
		{
			return (int)(glm::clamp(v, 0.0f, 1.0f) * 255.0f);
		};
		char colorKey[16];
		snprintf(colorKey, sizeof(colorKey), "%02X%02X%02X%02X",
			channelHex(color.r), channelHex(color.g), channelHex(color.b), channelHex(color.a));

		std::string key = texturePath.string() + "|" + colorKey;
		auto it = m_TextureCache.find(key);
		if (it != m_TextureCache.end())
			return it->second;

		Ref<Texture2D> result = nullptr;
		int w = 0, h = 0, channels = 0;
		stbi_uc* pixels = stbi_load(texturePath.string().c_str(), &w, &h, &channels, 4);
		if (pixels)
		{
			// Halve until the image fits 128px (odd sizes drop a pixel).
			int tw = w, th = h;
			while (tw > 128 || th > 128)
			{
				tw = (tw + 1) / 2;
				th = (th + 1) / 2;
			}

			std::vector<uint8_t> src(pixels, pixels + (size_t)w * h * 4);
			stbi_image_free(pixels);

			// Iterative 2x2 box downsample.
			int sw = w, sh = h;
			std::vector<uint8_t> dst;
			while (sw > tw || sh > th)
			{
				int dw = std::max(1, sw / 2), dh = std::max(1, sh / 2);
				dst.assign((size_t)dw * dh * 4, 0);
				for (int y = 0; y < dh; y++)
				{
					for (int x = 0; x < dw; x++)
					{
						for (int c = 0; c < 4; c++)
						{
							uint32_t sum = 0;
							int count = 0;
							for (int sy = 0; sy < 2; sy++)
							{
								int syy = std::min(y * 2 + sy, sh - 1);
								for (int sx = 0; sx < 2; sx++)
								{
									int sxx = std::min(x * 2 + sx, sw - 1);
									sum += src[(size_t)syy * sw * 4 + (size_t)sxx * 4 + c];
									count++;
								}
							}
							dst[(size_t)y * dw * 4 + (size_t)x * 4 + c] = (uint8_t)(sum / count);
						}
					}
				}
				src.swap(dst);
				sw = dw;
				sh = dh;
			}

			// Tint: rgb *= color.rgb, a *= color.a (sRGB multiply, same as the shader).
			for (size_t i = 0; i < src.size(); i += 4)
			{
				src[i + 0] = (uint8_t)(src[i + 0] * color.r);
				src[i + 1] = (uint8_t)(src[i + 1] * color.g);
				src[i + 2] = (uint8_t)(src[i + 2] * color.b);
				src[i + 3] = (uint8_t)(src[i + 3] * color.a);
			}

			result = Texture2D::Create((uint32_t)tw, (uint32_t)th);
			if (result)
				result->SetData(src.data(), (uint32_t)src.size());
		}

		if (!result)
		{
			// Decode failed - fall back to the untinted GPU texture, then the
			// file icon, so the failure itself is cached and never retried.
			result = Texture2D::Create(texturePath.string(), TextureFilter::Nearest);
			if (!result)
				result = m_FileIcon;
		}

		m_TextureCache[key] = result;
		return result;
	}

	void ContentBrowserPanel::OpenSpritesheetViewer(const std::filesystem::path& path)
	{
		m_SelectedSpritesheetPath = path;
		try {
			YAML::Node data = YAML::LoadFile(path.string());
			std::string texName = data["Spritesheet"].as<std::string>("");

			std::filesystem::path fullTexPath = path.parent_path() / texName;
			if (!std::filesystem::exists(fullTexPath)) fullTexPath = g_AssetPath / texName;
			if (std::filesystem::exists(fullTexPath))
			{
				m_SpritesheetTexture = Texture2D::Create(fullTexPath.string(), TextureFilter::Nearest);
				m_SpritesheetSubTextures.clear();
				m_SpritesheetRegions.clear();
				m_SpritesheetGroups.clear();
				m_SpritesheetTexAbsPath = std::filesystem::absolute(fullTexPath).string();

				// New named-region format (from Spritesheet Editor)
				if (data["Regions"] && data["Regions"].IsSequence())
				{
					const float texW = (float)m_SpritesheetTexture->GetWidth();
					const float texH = (float)m_SpritesheetTexture->GetHeight();
					for (auto regNode : data["Regions"])
					{
						SpritesheetRegionInfo info;
						info.Name = regNode["Name"].as<std::string>("Sprite");
						if (regNode["Min"].IsSequence() && regNode["Max"].IsSequence())
						{
							info.Min = { regNode["Min"][0].as<float>(0.0f), regNode["Min"][1].as<float>(0.0f) };
							info.Max = { regNode["Max"][0].as<float>(0.0f), regNode["Max"][1].as<float>(0.0f) };
						}
						if (regNode["Pivot"] && regNode["Pivot"].IsSequence())
							info.Pivot = { regNode["Pivot"][0].as<float>(-1.0f), regNode["Pivot"][1].as<float>(-1.0f) };
						m_SpritesheetRegions.push_back(info);

						// Build sub-texture from pixel rect (flip Y for OpenGL)
						glm::vec2 uvMin = { info.Min.x / texW, 1.0f - info.Max.y / texH };
						glm::vec2 uvMax = { info.Max.x / texW, 1.0f - info.Min.y / texH };
						auto sub = CreateRef<SubTexture2D>(m_SpritesheetTexture, uvMin, uvMax);
						if (sub) m_SpritesheetSubTextures.push_back(sub);
					}
				}
				else
				{
					// Legacy grid format
					m_SpritesheetCols = data["Columns"].as<int>(1);
					m_SpritesheetRows = data["Rows"].as<int>(1);
					float cellW = (float)m_SpritesheetTexture->GetWidth() / (float)m_SpritesheetCols;
					float cellH = (float)m_SpritesheetTexture->GetHeight() / (float)m_SpritesheetRows;
					int total = m_SpritesheetCols * m_SpritesheetRows;
					for (int i = 0; i < total; i++)
					{
						int col = i % m_SpritesheetCols;
						int row = m_SpritesheetRows - 1 - (i / m_SpritesheetCols);
						SpritesheetRegionInfo info;
						info.Name = "Sprite_" + std::to_string(i);
						info.Min = { (float)col * cellW, (float)(m_SpritesheetRows - 1 - row) * cellH };
						info.Max = { info.Min.x + cellW, info.Min.y + cellH };
						m_SpritesheetRegions.push_back(info);
						auto sub = SubTexture2D::CreateFromCoords(m_SpritesheetTexture, { (float)col, (float)row }, { cellW, cellH });
						if (sub) m_SpritesheetSubTextures.push_back(sub);
					}
				}

				// Named groups (animation clips) for the grouped viewer.
				if (data["Groups"] && data["Groups"].IsSequence())
				{
					for (auto groupNode : data["Groups"])
					{
						SpritesheetGroupInfo group;
						group.Name = groupNode["Name"].as<std::string>("Group");
						if (groupNode["Regions"] && groupNode["Regions"].IsSequence())
							for (auto idxNode : groupNode["Regions"])
								group.RegionIndices.push_back(idxNode.as<int>(-1));
						m_SpritesheetGroups.push_back(group);
					}
				}
			}
			m_ShowSpritesheetViewer = true;
		} catch (...) {}
	}

	void ContentBrowserPanel::OnImGuiRender()
	{
		ImGui::Begin("Content Browser");

		// Header navigation bar
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
		if (m_CurrentDirectory != g_AssetPath)
		{
			if (ImGui::Button(" <- Back "))
			{
				m_CurrentDirectory = m_CurrentDirectory.parent_path();
			}
			ImGui::SameLine();
		}

		// Breadcrumb path display
		std::string pathString = m_CurrentDirectory.string();
		ImGui::TextDisabled("Location: %s", pathString.c_str());
		ImGui::PopStyleVar();

		ImGui::Separator();

		static float padding = 16.0f;
		static float thumbnailSize = 72.0f;
		float cellSize = thumbnailSize + padding;

		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1)
			columnCount = 1;

		ImGui::Columns(columnCount, 0, false);

		uint32_t itemCount = 0;
		if (std::filesystem::exists(m_CurrentDirectory))
		{
			for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory))
			{
				const auto& path = directoryEntry.path();
				std::string ext = path.extension().string();
				for (auto& c : ext) c = (char)tolower(c);
				std::string fn = path.filename().string();

				// Hide internal engine/project files (.wfp, .wfk, .ini, .log, .yaml, dotfiles) from the user.
				// .spritesheet files are metadata of their PNG - the texture
				// entry owns them, so they stay out of the grid.
				if (ext == ".wfp" || ext == ".wfk" || ext == ".ini" || ext == ".log" || ext == ".yaml" || ext == ".yml" || ext == ".spritesheet" || (!fn.empty() && fn[0] == '.'))
					continue;

				itemCount++;
				auto relativePath = std::filesystem::relative(path, g_AssetPath);
				std::string filenameString = relativePath.filename().string();

				ImGui::PushID(filenameString.c_str());

				Ref<Texture2D> icon = directoryEntry.is_directory() ? m_DirectoryIcon : m_FileIcon;
				if (!directoryEntry.is_directory())
				{
					if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
					{
						std::string pathStr = path.string();
						auto it = m_TextureCache.find(pathStr);
						if (it != m_TextureCache.end() && it->second)
						{
							icon = it->second;
						}
						else
						{
							// Always load thumbnails as Nearest - clear any stale Linear entry first
							m_TextureCache.erase(pathStr);
							Ref<Texture2D> loadedTex = Texture2D::Create(pathStr, TextureFilter::Nearest);
							if (loadedTex)
							{
								m_TextureCache[pathStr] = loadedTex;
								icon = loadedTex;
							}
						}
					}
						else if (ext == ".prefab")
						{
							// The cache key includes the file's mtime: prefab
							// thumbnails must refresh when the prefab is saved
							// from the prefab editor (or edited by hand).
							const std::string filePath = path.string();
							std::error_code mtimeEc;
							const std::string cacheKey = filePath + "|@" +
								std::to_string(std::filesystem::last_write_time(path, mtimeEc).time_since_epoch().count());
							auto it = m_TextureCache.find(cacheKey);
							if (it != m_TextureCache.end() && it->second)
							{
								icon = it->second;
							}
							else
							{
								try {
									YAML::Node data = YAML::LoadFile(filePath);
									auto entityNode = data["Entity"];
									auto spriteNode = entityNode ? entityNode["SpriteRendererComponent"] : YAML::Node();
									auto circleNode = entityNode ? entityNode["CircleRendererComponent"] : YAML::Node();

									// Colors are read field-by-field: the glm::vec4 YAML
									// converters are local to SceneSerializer.cpp.
									auto readColorNode = [](const YAML::Node& n, glm::vec4& out) -> bool
									{
										if (n && n.IsSequence() && n.size() >= 4)
										{
											out = { n[0].as<float>(1.0f), n[1].as<float>(1.0f),
													n[2].as<float>(1.0f), n[3].as<float>(1.0f) };
											return true;
										}
										return false;
									};

									// 1. Sprite texture tinted with the sprite color -
									// the same combination the renderer draws with.
									glm::vec4 spriteColor(1.0f);
									readColorNode(spriteNode ? spriteNode["Color"] : YAML::Node(), spriteColor);

									if (spriteNode && spriteNode["TexturePath"])
									{
										std::string texRelPath = spriteNode["TexturePath"].as<std::string>();
										if (!texRelPath.empty())
										{
											// Resolve like the scene serializer does -
											// stored paths may or may not carry the
											// "Assets/" prefix.
											std::filesystem::path resolved = ResolveTexturePath(texRelPath);
											if (!resolved.empty() && std::filesystem::exists(resolved))
											{
												Ref<Texture2D> tinted = GetTintedThumbnail(resolved, spriteColor);
												if (tinted)
												{
													m_TextureCache[cacheKey] = tinted;
													icon = tinted;
												}
											}
										}
									}

									// 2. Color-only prefabs: sprite or circle color swatch.
									if (icon == m_FileIcon)
									{
										glm::vec4 color(1.0f);
										bool hasColor = readColorNode(spriteNode ? spriteNode["Color"] : YAML::Node(), color)
											|| readColorNode(circleNode ? circleNode["Color"] : YAML::Node(), color);
										if (hasColor)
										{
											Ref<Texture2D> swatch = GetColorSwatch(color);
											if (swatch)
											{
												m_TextureCache[cacheKey] = swatch;
												icon = swatch;
											}
										}
									}
								} catch (...) {}

								if (icon == m_FileIcon)
								{
									// Cache the "no thumbnail" result (as the
									// file icon) so prefabs without a sprite
									// don't re-parse YAML from disk EVERY frame.
									m_TextureCache[cacheKey] = m_FileIcon;
								}
							}
						}
				}

				ImVec2 iconSize = { thumbnailSize, thumbnailSize };
				if (icon && icon != m_DirectoryIcon && icon != m_FileIcon)
				{
					// Textures load with TextureFilter::Nearest already; the
					// old per-frame SetFilter(Nearest) here stalled the whole
					// Vulkan device once per thumbnail per frame.
					float aspect = (float)icon->GetWidth() / (float)icon->GetHeight();
					if (aspect > 0.0f)
					{
						if (aspect >= 1.0f)
							iconSize = ImVec2(thumbnailSize, thumbnailSize / aspect);
						else
							iconSize = ImVec2(thumbnailSize * aspect, thumbnailSize);
					}
				}

				bool isSelected = (m_SelectedItem == path);
				ImGui::PushStyleColor(ImGuiCol_Button, isSelected ? ImVec4{ 0.2f, 0.4f, 0.8f, 0.5f } : ImVec4{ 0, 0, 0, 0 });

				// Thumbnails are pixel art: request the backend's NEAREST
				// sampler for this draw. The ImGui Vulkan backend samples all
				// textures with its own Linear sampler unless a draw callback
				// switches it - the texture's own filter is ignored here.
				const bool isThumbnail = (icon && icon != m_DirectoryIcon && icon != m_FileIcon);
				ImDrawList* dl = ImGui::GetWindowDrawList();
				if (isThumbnail && ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest)
					dl->AddCallback(ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest, nullptr);

				ImGui::ImageButton("##", (ImTextureID)icon->GetImGuiTextureId(), iconSize, { 0, 1 }, { 1, 0 });

				if (isThumbnail)
				{
					// Reset back to the default (Linear) sampler for
					// everything drawn after this item.
					dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
				}

				// Drop entity onto item icon to create prefab
				if (ImGui::BeginDragDropTarget())
				{
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
					{
						UUID entityUUID = *(const UUID*)payload->Data;
						if (m_SceneContext)
						{
							Entity entity = m_SceneContext->GetEntityByUUID(entityUUID);
							if (entity)
							{
								std::string entityName = entity.GetComponent<TagComponent>().Tag;
								if (entityName.empty()) entityName = "Entity";

								std::filesystem::path prefabsDir = m_CurrentDirectory;
								if (std::filesystem::exists(g_AssetPath / "Prefabs"))
									prefabsDir = g_AssetPath / "Prefabs";

								std::filesystem::path prefabPath = prefabsDir / (entityName + ".prefab");
								int counter = 1;
								while (std::filesystem::exists(prefabPath))
								{
									prefabPath = prefabsDir / (entityName + std::to_string(counter++) + ".prefab");
								}

								SceneSerializer::SerializeEntityToPrefab(entity, prefabPath.string());
								WF_CORE_INFO("Saved entity '{0}' to prefab '{1}'", entityName, prefabPath.string());
							}
						}
					}
					ImGui::EndDragDropTarget();
				}

				if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				{
					m_SelectedItem = path;
				}

				if (ImGui::BeginDragDropSource())
				{
					const wchar_t* itemPath = relativePath.c_str();
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t), ImGuiCond_Once);
					ImGui::EndDragDropSource();
				}

				// Right click context menu on individual item (File / Folder)
				if (ImGui::BeginPopupContextItem())
				{
					m_SelectedItem = path;
					if (ImGui::MenuItem("Rename"))
					{
						m_ItemToRename = path;
						memset(m_RenameItemBuffer, 0, sizeof(m_RenameItemBuffer));
						strcpy_s(m_RenameItemBuffer, sizeof(m_RenameItemBuffer), path.filename().string().c_str());
						m_OpenRenameModal = true;
					}
					if (ImGui::MenuItem("Delete"))
					{
						m_ItemToDelete = path;
						m_OpenDeleteModal = true;
					}
					ImGui::EndPopup();
				}

				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (directoryEntry.is_directory())
					{
						m_CurrentDirectory /= path.filename();
						m_SelectedItem.clear();
					}
					else if (path.extension() == ".waffle")
					{
						if (m_OpenSceneCallback)
							m_OpenSceneCallback(path);
					}
					else if (ext == ".prefab")
					{
						// Double-click opens the prefab edit view (hierarchy
						// shows a Back button; saving writes the .prefab).
						if (m_OpenPrefabCallback)
							m_OpenPrefabCallback(path);
					}
					else if (ext == ".spritesheet")
					{
						OpenSpritesheetViewer(path);
					}
					else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")
					{
						// The spritesheet data belongs to the texture: double
						// click opens its sprite viewer when metadata exists.
						std::filesystem::path sheetPath = path;
						sheetPath.replace_extension(".spritesheet");
						if (std::filesystem::exists(sheetPath))
							OpenSpritesheetViewer(sheetPath);
					}
					else if (path.extension() == ".lua" || path.extension() == ".h" || path.extension() == ".cpp" || path.extension() == ".txt" || ext == ".glsl")
					{
						PlatformUtils::OpenFileInEditor(path.string());
					}
				}

				ImGui::TextWrapped("%s", filenameString.c_str());

				ImGui::NextColumn();

				ImGui::PopID();
			}
		}

		ImGui::Columns(1);

		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			m_SelectedItem.clear();
		}

		if (!m_SelectedItem.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_Delete))
		{
			m_ItemToDelete = m_SelectedItem;
			m_OpenDeleteModal = true;
		}

		// Right-click on blank space in Content Browser
		if (ImGui::BeginPopupContextWindow(0, 1 | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Folder"))
				{
					std::filesystem::path folderPath = m_CurrentDirectory / "NewFolder";
					int counter = 1;
					while (std::filesystem::exists(folderPath))
					{
						folderPath = m_CurrentDirectory / ("NewFolder" + std::to_string(counter++));
					}
					std::filesystem::create_directory(folderPath);
				}

				if (ImGui::MenuItem("Create Scene"))
				{
					std::filesystem::path scenePath = m_CurrentDirectory / "NewScene.waffle";
					int counter = 1;
					while (std::filesystem::exists(scenePath))
					{
						scenePath = m_CurrentDirectory / ("NewScene" + std::to_string(counter++) + ".waffle");
					}

					Ref<Scene> newScene = CreateRef<Scene>();
					newScene->SetName(scenePath.stem().string());
					SceneSerializer serializer(newScene);
					serializer.Serialize(scenePath.string());
				}

				if (ImGui::MenuItem("Lua Script"))
				{
					std::filesystem::path scriptPath = m_CurrentDirectory / "NewScript.lua";
					int counter = 1;
					while (std::filesystem::exists(scriptPath))
					{
						scriptPath = m_CurrentDirectory / ("NewScript" + std::to_string(counter++) + ".lua");
					}

					std::ofstream scriptFile(scriptPath);
					scriptFile << "-- Waffle Lua Script\n\n"
							   << "function OnCreate(entity)\n"
							   << "    -- Called when the script starts\n"
							   << "end\n\n"
							   << "function OnUpdate(entity, ts)\n"
							   << "    -- Called every frame during gameplay\n"
							   << "end\n\n"
							   << "function OnDestroy(entity)\n"
							   << "    -- Called when the script is destroyed\n"
							   << "end\n";
					scriptFile.close();
				}

				if (ImGui::MenuItem("Shader"))
				{
					std::filesystem::path shaderPath = m_CurrentDirectory / "NewShader.glsl";
					int counter = 1;
					while (std::filesystem::exists(shaderPath))
					{
						shaderPath = m_CurrentDirectory / ("NewShader" + std::to_string(counter++) + ".glsl");
					}

					std::ofstream shaderFile(shaderPath);
					shaderFile << k_NewShaderTemplate;
					shaderFile.close();
					WF_CORE_INFO("Created shader '{0}'", shaderPath.filename().string());
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		// Modal Dialog: Delete Item Confirmation
		if (m_OpenDeleteModal)
		{
			ImGui::OpenPopup("Delete Confirmation");
			m_OpenDeleteModal = false;
		}

		if (ImGui::BeginPopupModal("Delete Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Are you sure you want to delete '%s'?", m_ItemToDelete.filename().string().c_str());
			ImGui::TextDisabled("This action cannot be undone.");
			ImGui::Separator();

			if (ImGui::Button("Delete", ImVec2(120, 0)))
			{
				std::error_code ec;
				if (std::filesystem::exists(m_ItemToDelete, ec))
				{
					std::filesystem::remove_all(m_ItemToDelete, ec);
					if (ec)
						WF_CORE_ERROR("Failed to delete '{0}': {1}", m_ItemToDelete.string(), ec.message());
					// Evict cached thumbnails under the deleted path.
					for (auto it = m_TextureCache.begin(); it != m_TextureCache.end(); )
					{
						if (it->first.rfind(m_ItemToDelete.string(), 0) == 0)
							it = m_TextureCache.erase(it);
						else
							++it;
					}
				}
				if (m_SelectedItem == m_ItemToDelete)
					m_SelectedItem.clear();
				m_ItemToDelete.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				m_ItemToDelete.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// Modal Dialog: Rename Item
		if (m_OpenRenameModal)
		{
			ImGui::OpenPopup("Rename Item");
			m_OpenRenameModal = false;
		}

		if (ImGui::BeginPopupModal("Rename Item", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Enter new name:");
			ImGui::InputText("##NewItemName", m_RenameItemBuffer, sizeof(m_RenameItemBuffer));

			if (ImGui::Button("Rename", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Enter))
			{
				std::string newName = m_RenameItemBuffer;
				if (!newName.empty() && std::filesystem::exists(m_ItemToRename))
				{
					std::filesystem::path newPath = m_ItemToRename.parent_path() / newName;
					if (!newPath.has_extension() && m_ItemToRename.has_extension())
					{
						newPath += m_ItemToRename.extension();
					}

					if (newPath != m_ItemToRename)
					{
						// Never silently destroy an existing file: MSVC's
						// rename uses MOVEFILE_REPLACE_EXISTING.
						if (std::filesystem::exists(newPath))
						{
							WF_CORE_ERROR("Rename failed: '{0}' already exists", newPath.filename().string());
						}
						else
						{
							std::error_code ec;
							std::filesystem::path oldPath = m_ItemToRename;
							std::filesystem::rename(m_ItemToRename, newPath, ec);
							if (ec)
							{
								// Invalid characters / locked file throw
								// out of the ImGui render loop otherwise.
								WF_CORE_ERROR("Rename failed: {0}", ec.message());
							}
							else
							{
								// Move cached thumbnail to the new key.
								auto it = m_TextureCache.find(oldPath.string());
								if (it != m_TextureCache.end())
								{
									m_TextureCache[newPath.string()] = it->second;
									m_TextureCache.erase(it);
								}

								if (oldPath.extension() == ".waffle" && m_SceneRenamedCallback)
								{
									m_SceneRenamedCallback(oldPath, newPath);
								}
							}
						}
					}
				}
				m_ItemToRename.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				m_ItemToRename.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		// Modal Dialog: Create Spritesheet
		if (m_OpenSliceModal)
		{
			ImGui::OpenPopup("Create Spritesheet");
			m_OpenSliceModal = false;
		}

		if (ImGui::BeginPopupModal("Create Spritesheet", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Slicing image: %s", m_ItemToSlice.filename().string().c_str());
			ImGui::Separator();

			ImGui::DragInt("Columns (Grid Width)", &m_SliceColumns, 1.0f, 1, 64);
			ImGui::DragInt("Rows (Grid Height)", &m_SliceRows, 1.0f, 1, 64);

			ImGui::Spacing();
			if (ImGui::Button("Slice & Create Asset", ImVec2(180, 0)))
			{
				std::filesystem::path sheetYamlPath = m_ItemToSlice.parent_path() / (m_ItemToSlice.stem().string() + ".spritesheet");
				YAML::Emitter out;
				out << YAML::BeginMap;
				out << YAML::Key << "Spritesheet" << YAML::Value << m_ItemToSlice.filename().string();
				out << YAML::Key << "Columns" << YAML::Value << m_SliceColumns;
				out << YAML::Key << "Rows" << YAML::Value << m_SliceRows;
				out << YAML::EndMap;

				std::ofstream fout(sheetYamlPath);
				fout << out.c_str();
				WF_CORE_INFO("Saved spritesheet asset: {0}", sheetYamlPath.string());

				m_ItemToSlice.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(100, 0)))
			{
				m_ItemToSlice.clear();
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		// =========================================================================
		//  SPRITESHEET SPRITE VIEWER  (grouped filmstrip / sheet overlay)
		// =========================================================================
		if (m_ShowSpritesheetViewer && !m_SpritesheetSubTextures.empty() && m_SpritesheetTexture)
		{
			ImGui::Separator();

			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

			ImGui::Text("Sprites  -  %s  (%zu sprites, %zu groups)",
				m_SelectedSpritesheetPath.filename().string().c_str(),
				m_SpritesheetSubTextures.size(), m_SpritesheetGroups.size());
			ImGui::SameLine();
			if (ImGui::Button(m_SpritesheetShowSheet ? "Grid View" : "Sheet View", ImVec2(86.0f, 22.0f)))
				m_SpritesheetShowSheet = !m_SpritesheetShowSheet;
			ImGui::SameLine();
			if (ImGui::Button("Edit Sprites", ImVec2(92.0f, 22.0f)))
			{
				if (m_OpenSpritesheetEditorCallback)
					m_OpenSpritesheetEditorCallback(m_SelectedSpritesheetPath);
			}
			ImGui::SameLine();
			if (ImGui::Button("x##CloseViewer", ImVec2(22.0f, 22.0f)))
			{
				m_ShowSpritesheetViewer = false;
				m_SpritesheetSubTextures.clear();
				m_SpritesheetRegions.clear();
				m_SpritesheetGroups.clear();
				m_SpritesheetTexture = nullptr;
				m_SpritesheetTexAbsPath.clear();
			}

			ImGui::PopStyleVar();

			// Per-region group index (-1 = ungrouped).
			std::vector<int> regionGroup(m_SpritesheetRegions.size(), -1);
			for (int g = 0; g < (int)m_SpritesheetGroups.size(); g++)
				for (int idx : m_SpritesheetGroups[g].RegionIndices)
					if (idx >= 0 && idx < (int)regionGroup.size())
						regionGroup[idx] = g;

			auto GroupColor = [this](int g) -> ImU32
			{
				if (g < 0)
					return IM_COL32(160, 160, 160, 255);
				float hue = (float)(g * 47) / 360.0f;
				float r, gg, b;
				ImGui::ColorConvertHSVtoRGB(hue, 0.72f, 0.95f, r, gg, b);
				return IM_COL32((int)(r * 255), (int)(gg * 255), (int)(b * 255), 255);
			};

			if (m_SpritesheetShowSheet)
			{
				// ── Sheet view: the whole texture with grouped region
				//    rectangles drawn over it (no sprite names).
				ImGui::BeginChild("##SheetView", ImVec2(0, 340.0f), true,
					ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

				ImDrawList* dl = ImGui::GetWindowDrawList();
				const float texW = (float)m_SpritesheetTexture->GetWidth();
				const float texH = (float)m_SpritesheetTexture->GetHeight();
				const float availX = ImGui::GetContentRegionAvail().x;
				const float scale = std::min((availX - 16.0f) / texW, 300.0f / texH);
				const ImVec2 disp = { texW * scale, texH * scale };
				const ImVec2 tl = {
					ImGui::GetCursorScreenPos().x + (availX - disp.x) * 0.5f,
					ImGui::GetCursorScreenPos().y + 8.0f
				};

				ImGuiLayer::BeginTextureSamplerPassthrough(dl);
				dl->AddImage((ImTextureID)m_SpritesheetTexture->GetImGuiTextureId(),
					tl, { tl.x + disp.x, tl.y + disp.y }, ImVec2(0, 1), ImVec2(1, 0));
				ImGuiLayer::EndTextureSamplerPassthrough(dl);

				auto PixelToScreen = [&](const glm::vec2& px)
				{
					return ImVec2{ tl.x + (px.x / texW) * disp.x,
						tl.y + (px.y / texH) * disp.y };
				};

				for (int i = 0; i < (int)m_SpritesheetRegions.size(); i++)
				{
					const auto& reg = m_SpritesheetRegions[i];
					const ImU32 col = GroupColor(i < (int)regionGroup.size() ? regionGroup[i] : -1);
					const ImVec2 pMin = PixelToScreen(reg.Min);
					const ImVec2 pMax = PixelToScreen(reg.Max);
					dl->AddRectFilled(pMin, pMax, (col & 0x00FFFFFF) | (28u << 24));
					dl->AddRect(pMin, pMax, col, 0.0f, 0, 2.0f);

					// Group label on the first region of each group.
					const int g = i < (int)regionGroup.size() ? regionGroup[i] : -1;
					bool firstOfGroup = (g >= 0) && (m_SpritesheetGroups[g].RegionIndices.empty()
						|| m_SpritesheetGroups[g].RegionIndices.front() == i);
					if (firstOfGroup)
					{
						dl->AddRectFilled({ pMin.x, pMin.y - 15.0f }, { pMin.x + 12.0f, pMin.y - 3.0f }, col, 2.0f);
						dl->AddText({ pMin.x + 16.0f, pMin.y - 17.0f }, IM_COL32(235, 235, 235, 255),
							m_SpritesheetGroups[g].Name.c_str());
					}
				}

				ImGui::Dummy(ImVec2(0, disp.y + 16.0f));
				ImGui::EndChild();
			}
			else
			{
				// ── Grouped filmstrip: one labeled row per group, thumbnails
				//    only (names in tooltips). Ungrouped sprites last.
				ImGui::BeginChild("##SpriteViewer", ImVec2(0, 150.0f), true,
					ImGuiWindowFlags_HorizontalScrollbar);

				const float sprThumb = 64.0f;
				const std::string& texPathForPayload = m_SpritesheetTexAbsPath;

				auto drawRegionThumb = [&](int i)
				{
					const auto& sub = m_SpritesheetSubTextures[i];
					const auto& region = (i < (int)m_SpritesheetRegions.size()) ? m_SpritesheetRegions[i] : SpritesheetRegionInfo{};

					ImTextureID texID = (ImTextureID)m_SpritesheetTexture->GetImGuiTextureId();
					const glm::vec2* uvs = sub->GetTexCoords();

					ImVec2 dispSize = { sprThumb, sprThumb };
					float pw = region.Max.x - region.Min.x;
					float ph = region.Max.y - region.Min.y;
					if (pw > 0.0f && ph > 0.0f)
					{
						float asp = pw / ph;
						if (asp >= 1.0f)
							dispSize = { sprThumb, sprThumb / asp };
						else
							dispSize = { sprThumb * asp, sprThumb };
					}

					ImDrawList* dl = ImGui::GetWindowDrawList();
					ImGuiLayer::BeginTextureSamplerPassthrough(dl);

					ImGui::ImageButton("##SprBtn", texID, dispSize,
						ImVec2(uvs[3].x, uvs[3].y), ImVec2(uvs[1].x, uvs[1].y));

					ImGuiLayer::EndTextureSamplerPassthrough(dl);

					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
					{
						char payloadBuf[1024];
						if (region.Pivot.x >= 0.0f && region.Pivot.y >= 0.0f)
							snprintf(payloadBuf, sizeof(payloadBuf), "%s|%.0f,%.0f,%.0f,%.0f|%.3f,%.3f",
								texPathForPayload.c_str(),
								region.Min.x, region.Min.y, region.Max.x, region.Max.y,
								region.Pivot.x, region.Pivot.y);
						else
							snprintf(payloadBuf, sizeof(payloadBuf), "%s|%.0f,%.0f,%.0f,%.0f",
								texPathForPayload.c_str(),
								region.Min.x, region.Min.y, region.Max.x, region.Max.y);
						ImGui::SetDragDropPayload("SPRITESHEET_FRAME_ITEM",
							payloadBuf, strlen(payloadBuf) + 1);

						ImGuiLayer::BeginTextureSamplerPassthrough(dl);
						ImGui::Image(texID, ImVec2(sprThumb, sprThumb),
							ImVec2(uvs[3].x, uvs[3].y), ImVec2(uvs[1].x, uvs[1].y));
						ImGuiLayer::EndTextureSamplerPassthrough(dl);

						ImGui::Text("%s", region.Name.c_str());
						ImGui::EndDragDropSource();
					}

					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("%s\n%.0f x %.0f px", region.Name.c_str(), pw, ph);
				};

				bool anyUngrouped = false;
				for (int i = 0; i < (int)m_SpritesheetRegions.size(); i++)
					if (i >= (int)regionGroup.size() || regionGroup[i] < 0) { anyUngrouped = true; break; }

				const int groupCount = (int)m_SpritesheetGroups.size() + (anyUngrouped ? 1 : 0);
				for (int g = 0; g < groupCount; g++)
				{
					const bool ungrouped = (g == (int)m_SpritesheetGroups.size());
					const std::string& groupName = ungrouped
						? std::string("Ungrouped") : m_SpritesheetGroups[g].Name;
					const std::vector<int>* indices = ungrouped
						? nullptr : &m_SpritesheetGroups[g].RegionIndices;

					ImGui::TextColored(ImVec4(0.92f, 0.95f, 0.55f, 0.9f), "%s", groupName.c_str());
					ImGui::SameLine(0.0f, 8.0f);

					int shown = 0;
					for (int i = 0; i < (int)m_SpritesheetRegions.size(); i++)
					{
						const bool inGroup = indices
							? std::find(indices->begin(), indices->end(), i) != indices->end()
							: (i >= (int)regionGroup.size() || regionGroup[i] < 0);
						if (!inGroup)
							continue;

						if (shown++ > 0)
							ImGui::SameLine(0.0f, 6.0f);
						ImGui::PushID(i);
						ImGui::BeginGroup();
						drawRegionThumb(i);
						ImGui::EndGroup();
						ImGui::PopID();
					}
					if (shown == 0)
						ImGui::TextDisabled(" (no sprites)");
				}

				ImGui::EndChild();
			}
			ImGui::PopStyleColor();
		}


		// A zero-size Dummy can never be hovered, so the previous background
		// drop target was unreachable dead code - use a real filler item that
		// covers the remaining panel space.
		ImVec2 remaining = ImGui::GetContentRegionAvail();
		remaining.x = glm::max(remaining.x, 1.0f);
		remaining.y = glm::max(remaining.y, 1.0f);
		ImGui::Dummy(remaining);
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_HIERARCHY_ENTITY"))
			{
				UUID entityUUID = *(const UUID*)payload->Data;
				if (m_SceneContext)
				{
					Entity entity = m_SceneContext->GetEntityByUUID(entityUUID);
					if (entity)
					{
						std::string entityName = entity.GetComponent<TagComponent>().Tag;
						if (entityName.empty()) entityName = "Entity";

						std::filesystem::path prefabsDir = m_CurrentDirectory;
						if (std::filesystem::exists(g_AssetPath / "Prefabs"))
							prefabsDir = g_AssetPath / "Prefabs";

						std::filesystem::path prefabPath = prefabsDir / (entityName + ".prefab");
						int counter = 1;
						while (std::filesystem::exists(prefabPath))
						{
							prefabPath = prefabsDir / (entityName + std::to_string(counter++) + ".prefab");
						}

						SceneSerializer::SerializeEntityToPrefab(entity, prefabPath.string());
						WF_CORE_INFO("Saved entity '{0}' to prefab '{1}'", entityName, prefabPath.string());
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
	}
}