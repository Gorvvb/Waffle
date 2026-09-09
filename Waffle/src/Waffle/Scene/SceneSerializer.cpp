#include "wfpch.h"
#include "SceneSerializer.h"
#include "Waffle/Scripting/LuaScriptEngine.h"
#include "Waffle/Core/VFS.h"

#include "Entity.h"
#include "Components.h"

#include <fstream>

#include <yaml-cpp/yaml.h>

namespace YAML {

	template<>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
			{
				if (node.IsScalar())
				{
					float val = node.as<float>();
					rhs = glm::vec2(val);
					return true;
				}
				return false;
			}

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec3& rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template<>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::vec4& rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;
			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};
	template<>
	struct convert<glm::uvec2>
	{
		static Node encode(const glm::uvec2& rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node& node, glm::uvec2& rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;
			rhs.x = node[0].as<uint32_t>();
			rhs.y = node[1].as<uint32_t>();
			return true;
		}
	};
}

namespace Waffle {

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::uvec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

	static std::string Rigidbody2DBodyTypeToString(Rigidbody2DComponent::BodyType bodyType)
	{
		switch (bodyType)
		{
			case Rigidbody2DComponent::BodyType::Static:	return "Static";
			case Rigidbody2DComponent::BodyType::Dynamic:	return "Dynamic";
			case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
		}

		WF_CORE_ASSERT(false, "Unknown body type!");
		return {};
	}

	static Rigidbody2DComponent::BodyType Rigidbody2DBodyTypeFromString(const std::string& bodyTypeString)
	{
		if (bodyTypeString == "Static") return Rigidbody2DComponent::BodyType::Static;
		if (bodyTypeString == "Dynamic") return Rigidbody2DComponent::BodyType::Dynamic;
		if (bodyTypeString == "Kinematic") return Rigidbody2DComponent::BodyType::Kinematic;

		WF_CORE_ASSERT(false, "Unknown body type!");
		return Rigidbody2DComponent::BodyType::Static;
	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{

	}

	static void SerializeEntity(YAML::Emitter& out, Entity entity)
	{
		WF_CORE_ASSERT(entity.HasComponent<IDComponent>());
		out << YAML::BeginMap; // Entity
		out << YAML::Key << "Entity" << YAML::Value << entity.GetUUID();

		if (entity.HasComponent<TagComponent>())
		{
			out << YAML::Key << "TagComponent";
			out << YAML::BeginMap; // TagComponent

			auto& tag = entity.GetComponent<TagComponent>().Tag;
			out << YAML::Key << "Tag" << YAML::Value << tag;

			out << YAML::EndMap; // TagComponent
		}

		if (entity.HasComponent<TransformComponent>())
		{
			out << YAML::Key << "TransformComponent";
			out << YAML::BeginMap; // TransformComponent

			auto& tc = entity.GetComponent<TransformComponent>();
			out << YAML::Key << "Translation" << YAML::Value << tc.Translation;
			out << YAML::Key << "Rotation" << YAML::Value << tc.Rotation;
			out << YAML::Key << "Scale" << YAML::Value << tc.Scale;

			out << YAML::EndMap; // TransformComponent
		}

		if (entity.HasComponent<CameraComponent>())
		{
			out << YAML::Key << "CameraComponent";
			out << YAML::BeginMap; // CameraComponent

			auto& cameraComponent = entity.GetComponent<CameraComponent>();
			auto& camera = cameraComponent.Camera;

			out << YAML::Key << "Camera" << YAML::Value;
			out << YAML::BeginMap; // Camera
			out << YAML::Key << "ProjectionType" << YAML::Value << (int)camera.GetProjectionType();
			out << YAML::Key << "PerspectiveFov" << YAML::Value << camera.GetPerspectiveVerticalFOV();
			out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.GetPerspectiveNearClip();
			out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.GetPerspectiveFarClip();
			out << YAML::Key << "OrthographicSize" << YAML::Value << camera.GetOrthographicSize();
			out << YAML::Key << "OrthographicNear" << YAML::Value << camera.GetOrthographicNearClip();
			out << YAML::Key << "OrthographicFar" << YAML::Value << camera.GetOrthographicFarClip();
			out << YAML::EndMap; // Camera

			out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
			out << YAML::Key << "FixedAspectRatio" << YAML::Value << cameraComponent.FixedAspectRatio;
			// Fixed-aspect cameras are never driven by OnViewportResize - the
			// aspect must be persisted or it comes back as 0 (degenerate
			// projection) after a reload.
			if (cameraComponent.FixedAspectRatio)
				out << YAML::Key << "AspectRatio" << YAML::Value << camera.GetAspectRatio();
			out << YAML::Key << "BackgroundColor" << YAML::Value << cameraComponent.BackgroundColor;
			out << YAML::Key << "BackgroundImagePath" << YAML::Value << GetNormalizedAssetPath(cameraComponent.BackgroundImagePath);
			out << YAML::Key << "BackgroundTilingFactor" << YAML::Value << cameraComponent.BackgroundTilingFactor;
			out << YAML::Key << "BackgroundFilterMode" << YAML::Value << static_cast<int>(cameraComponent.BackgroundFilterMode);

			out << YAML::EndMap; // CameraComponent
		}

		if (entity.HasComponent<SpriteRendererComponent>())
		{
			out << YAML::Key << "SpriteRendererComponent";
			out << YAML::BeginMap; // SpriteRendererComponent

			auto& spriteRendererComponent = entity.GetComponent<SpriteRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << spriteRendererComponent.Color;

			if (spriteRendererComponent.Texture)
				out << YAML::Key << "TexturePath" << YAML::Value << GetNormalizedAssetPath(spriteRendererComponent.Texture->GetPath());

			out << YAML::Key << "FilterMode" << YAML::Value << static_cast<int>(spriteRendererComponent.FilterMode);
			out << YAML::Key << "TilingFactor" << YAML::Value << spriteRendererComponent.TilingFactor;
			out << YAML::Key << "SortingLayer" << YAML::Value << spriteRendererComponent.SortingLayer;
			out << YAML::Key << "SortingOrder" << YAML::Value << spriteRendererComponent.SortingOrder;

			out << YAML::EndMap; // SpriteRendererComponent
		}

		if (entity.HasComponent<CircleRendererComponent>())
		{
			out << YAML::Key << "CircleRendererComponent";
			out << YAML::BeginMap; // CircleRendererComponent

			auto& circleRendererComponent = entity.GetComponent<CircleRendererComponent>();
			out << YAML::Key << "Color" << YAML::Value << circleRendererComponent.Color;
			out << YAML::Key << "Thickness" << YAML::Value << circleRendererComponent.Thickness;
			out << YAML::Key << "Fade" << YAML::Value << circleRendererComponent.Fade;
			out << YAML::Key << "SortingLayer" << YAML::Value << circleRendererComponent.SortingLayer;
			out << YAML::Key << "SortingOrder" << YAML::Value << circleRendererComponent.SortingOrder;

			out << YAML::EndMap; // CircleRendererComponent
		}

		if (entity.HasComponent<RelationshipComponent>())
		{
			out << YAML::Key << "RelationshipComponent";
			out << YAML::BeginMap;
			auto& rc = entity.GetComponent<RelationshipComponent>();
			out << YAML::Key << "Parent" << YAML::Value << rc.Parent;
			out << YAML::Key << "Children" << YAML::Value << YAML::BeginSeq;
			for (auto childUUID : rc.Children)
				out << childUUID;
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<ScriptComponent>())
		{
			out << YAML::Key << "ScriptComponent";
			out << YAML::BeginMap;
			auto& sc = entity.GetComponent<ScriptComponent>();
			out << YAML::Key << "ClassName" << YAML::Value << sc.ClassName;
			out << YAML::Key << "ScriptPaths" << YAML::Value << YAML::BeginSeq;
			for (const auto& path : sc.ScriptPaths)
				out << path;
			out << YAML::EndSeq;

			if (!sc.Fields.empty())
			{
				// sc.Fields is an unordered_map - emit in sorted key order so
				// the file is deterministic across loads and machines.
				std::vector<const std::pair<const std::string, std::vector<LuaField>>*> sortedFields;
				sortedFields.reserve(sc.Fields.size());
				for (const auto& entry : sc.Fields)
					sortedFields.push_back(&entry);
				std::sort(sortedFields.begin(), sortedFields.end(),
					[](auto* a, auto* b) { return a->first < b->first; });

				out << YAML::Key << "PublicFields" << YAML::Value << YAML::BeginSeq;
				for (const auto* entry : sortedFields)
				{
					for (const auto& field : entry->second)
					{
						out << YAML::BeginMap;
						out << YAML::Key << "Script" << YAML::Value << entry->first;
						out << YAML::Key << "Name" << YAML::Value << field.Name;
						out << YAML::Key << "Type" << YAML::Value << (int)field.Type;
						out << YAML::Key << "UserModified" << YAML::Value << field.UserModified;
						switch (field.Type)
						{
						case LuaFieldType::Float:  out << YAML::Key << "Value" << YAML::Value << field.FloatVal;  break;
						case LuaFieldType::Int:    out << YAML::Key << "Value" << YAML::Value << field.IntVal;    break;
						case LuaFieldType::Bool:   out << YAML::Key << "Value" << YAML::Value << field.BoolVal;   break;
						case LuaFieldType::String: out << YAML::Key << "Value" << YAML::Value << field.StringVal; break;
						}
						out << YAML::EndMap;
					}
				}
				out << YAML::EndSeq;
			}

			out << YAML::EndMap;
		}

		if (entity.HasComponent<LifetimeComponent>())
		{
			out << YAML::Key << "LifetimeComponent";
			out << YAML::BeginMap;
			auto& lc = entity.GetComponent<LifetimeComponent>();
			out << YAML::Key << "Lifetime" << YAML::Value << lc.Lifetime;
			out << YAML::EndMap;
		}

		if (entity.HasComponent<Rigidbody2DComponent>())
		{
			out << YAML::Key << "Rigidbody2DComponent";
			out << YAML::BeginMap; // Rigidbody2DComponent

			auto& rb2dComponent = entity.GetComponent<Rigidbody2DComponent>();
			out << YAML::Key << "BodyType" << YAML::Value << Rigidbody2DBodyTypeToString(rb2dComponent.Type);
			out << YAML::Key << "FixedRotation" << YAML::Value << rb2dComponent.FixedRotation;
			out << YAML::Key << "Mass" << YAML::Value << rb2dComponent.Mass;

			out << YAML::EndMap; // Rigidbody2DComponent
		}

		if (entity.HasComponent<BoxCollider2DComponent>())
		{
			out << YAML::Key << "BoxCollider2DComponent";
			out << YAML::BeginMap; // BoxCollider2DComponent

			auto& bc2dComponent = entity.GetComponent<BoxCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << bc2dComponent.Offset;
			out << YAML::Key << "Size" << YAML::Value << bc2dComponent.Size;
			out << YAML::Key << "Density" << YAML::Value << bc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << bc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << bc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << bc2dComponent.RestitutionThreshold;
			out << YAML::Key << "IsTrigger" << YAML::Value << bc2dComponent.IsTrigger;

			out << YAML::EndMap; // BoxCollider2DComponent
		}

		if (entity.HasComponent<CircleCollider2DComponent>())
		{
			out << YAML::Key << "CircleCollider2DComponent";
			out << YAML::BeginMap; // CircleCollider2DComponent

			auto& cc2dComponent = entity.GetComponent<CircleCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << cc2dComponent.Offset;
			out << YAML::Key << "Radius" << YAML::Value << cc2dComponent.Radius;
			out << YAML::Key << "Density" << YAML::Value << cc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << cc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << cc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << cc2dComponent.RestitutionThreshold;
			out << YAML::Key << "IsTrigger" << YAML::Value << cc2dComponent.IsTrigger;

			out << YAML::EndMap; // CircleCollider2DComponent
		}

		if (entity.HasComponent<AnimatorComponent>())
		{
			out << YAML::Key << "AnimatorComponent";
			out << YAML::BeginMap; // AnimatorComponent

			auto& animator = entity.GetComponent<AnimatorComponent>();
			out << YAML::Key << "CurrentClip" << YAML::Value << animator.CurrentClip;

			// animator.Clips is an unordered_map - emit sorted for
			// deterministic output.
			std::vector<const std::pair<const std::string, AnimationClip>*> sortedClips;
			sortedClips.reserve(animator.Clips.size());
			for (const auto& clipEntry : animator.Clips)
				sortedClips.push_back(&clipEntry);
			std::sort(sortedClips.begin(), sortedClips.end(),
				[](auto* a, auto* b) { return a->first < b->first; });

			out << YAML::Key << "Clips" << YAML::Value << YAML::BeginSeq;
			for (const auto* clipEntry : sortedClips)
			{
				const auto& clip = clipEntry->second;
				out << YAML::BeginMap;
				out << YAML::Key << "Name" << YAML::Value << clip.Name;
				out << YAML::Key << "TexturePath" << YAML::Value << GetNormalizedAssetPath(clip.TexturePath);
				out << YAML::Key << "Columns" << YAML::Value << clip.Columns;
				out << YAML::Key << "Rows" << YAML::Value << clip.Rows;
				out << YAML::Key << "StartFrame" << YAML::Value << clip.StartFrame;
				out << YAML::Key << "EndFrame" << YAML::Value << clip.EndFrame;
				out << YAML::Key << "FPS" << YAML::Value << clip.FPS;
				out << YAML::Key << "Loop" << YAML::Value << clip.Loop;

				out << YAML::Key << "KeyframeImagePaths" << YAML::Value << YAML::BeginSeq;
				for (auto& kfPath : clip.KeyframeImagePaths)
					out << GetNormalizedAssetPath(kfPath);
				out << YAML::EndSeq;

				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap; // AnimatorComponent
		}

		if (entity.HasComponent<PolygonCollider2DComponent>())
		{
			out << YAML::Key << "PolygonCollider2DComponent";
			out << YAML::BeginMap;
			auto& pc2dComponent = entity.GetComponent<PolygonCollider2DComponent>();
			out << YAML::Key << "Offset" << YAML::Value << pc2dComponent.Offset;
			out << YAML::Key << "Density" << YAML::Value << pc2dComponent.Density;
			out << YAML::Key << "Friction" << YAML::Value << pc2dComponent.Friction;
			out << YAML::Key << "Restitution" << YAML::Value << pc2dComponent.Restitution;
			out << YAML::Key << "RestitutionThreshold" << YAML::Value << pc2dComponent.RestitutionThreshold;
			out << YAML::Key << "IsTrigger" << YAML::Value << pc2dComponent.IsTrigger;
			out << YAML::Key << "Vertices" << YAML::Value << YAML::BeginSeq;
			for (const auto& v : pc2dComponent.Vertices)
				out << v;
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}

		out << YAML::EndMap; // Entity
	}

	bool SceneSerializer::Serialize(const std::string& filepath)
	{
		YAML::Emitter out;
		out << YAML::BeginMap; // Scene
		out << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		auto view = m_Scene->m_Registry.view<TagComponent>();
		view.each([&](auto entityID, TagComponent& tagComponent)
		{
			Entity entity = { entityID, m_Scene.get() };
			if (!entity)
				return;

			SerializeEntity(out, entity);
		});
		out << YAML::EndSeq;
		out << YAML::EndMap; // Scene

		std::ofstream fout(filepath);
		fout << out.c_str();
		if (!fout)
		{
			WF_CORE_ERROR("Failed to write scene file '{0}' (disk full or file locked?)", filepath);
			return false;
		}
		return true;
	}

	void SceneSerializer::SerializeRuntime(const std::string& filepath)
	{
		// Not implemented yet
		WF_CORE_ASSERT(false);
	}

	bool SceneSerializer::Deserialize(const std::string& filepath)
	{
		YAML::Node data;
		try
		{
			std::string yamlContent = VFS::ReadFileAsString(filepath);
			if (!yamlContent.empty())
			{
				data = YAML::Load(yamlContent);
			}
			else
			{
				data = YAML::LoadFile(filepath);
			}
		}
		catch (const YAML::Exception& e)
		{
			// Catches ParserException, BadFile, InvalidNode, BadConversion -
			// a missing/corrupt scene must fail to load, not kill the process.
			WF_CORE_ERROR("Failed to load scene file '{0}'\n     {1}", filepath, e.what());
			return false;
		}

		if (!data["Scene"])
			return false;

		std::string sceneName;
		try { sceneName = data["Scene"].as<std::string>(); }
		catch (const YAML::Exception&) { sceneName = "Scene"; }
		m_Scene->SetName(sceneName);
		WF_CORE_TRACE("Deserializing scene '{0}'", sceneName);

		auto entities = data["Entities"];
		if (entities)
		{
			// The recursive fallback scan below walks the whole asset tree
			// per unresolved script path - cache it once per deserialize.
			std::unordered_map<std::string, std::filesystem::path> scriptScanCache;
			bool scriptScanDone = false;
			auto findScriptAnywhere = [&](const std::string& scriptPath) -> std::filesystem::path
			{
				if (!scriptScanDone)
				{
					scriptScanDone = true;
					std::error_code ec;
					for (auto& entry : std::filesystem::recursive_directory_iterator("Assets", ec))
					{
						if (entry.is_regular_file(ec))
						{
							std::string fn = entry.path().filename().string();
							if (fn.size() >= 4 && fn.compare(fn.size() - 4, 4, ".lua") == 0)
								scriptScanCache[fn] = entry.path();
						}
					}
				}
				auto it = scriptScanCache.find(scriptPath);
				return (it != scriptScanCache.end()) ? it->second : std::filesystem::path();
			};

			for (auto entity : entities)
			{
			// One malformed entity (missing key, wrong type) must not take
			// the whole load down - skip it and keep going.
			try
			{
				uint64_t uuid = entity["Entity"].as<uint64_t>();

				std::string name;
				auto tagComponent = entity["TagComponent"];
				if (tagComponent)
					name = tagComponent["Tag"].as<std::string>();
				WF_CORE_TRACE("Deserialized entity with ID = {0}, name = {1}", uuid, name);

				Entity deserializedEntity = m_Scene->CreateEntityWithUUID(uuid, name);

				auto relationshipComponent = entity["RelationshipComponent"];
				if (relationshipComponent)
				{
					auto& rc = deserializedEntity.AddComponent<RelationshipComponent>();
					rc.Parent = relationshipComponent["Parent"].as<uint64_t>();
					auto children = relationshipComponent["Children"];
					if (children)
					{
						for (auto child : children)
							rc.Children.push_back(child.as<uint64_t>());
					}
				}

				auto scriptComponent = entity["ScriptComponent"];
				if (scriptComponent)
				{
					auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
					if (scriptComponent["ClassName"])
						sc.ClassName = scriptComponent["ClassName"].as<std::string>();

					auto scriptPaths = scriptComponent["ScriptPaths"];
					if (scriptPaths)
					{
						for (auto pathNode : scriptPaths)
							sc.ScriptPaths.push_back(pathNode.as<std::string>());
					}
					if (sc.ScriptPaths.empty() && !sc.ClassName.empty())
						sc.ScriptPaths.push_back(sc.ClassName);

					auto publicFields = scriptComponent["PublicFields"];
					if (publicFields)
					{
						for (auto fieldNode : publicFields)
						{
							LuaField field;
							std::string scriptPath = fieldNode["Script"].as<std::string>();
							field.Name = fieldNode["Name"].as<std::string>();
							field.Type = (LuaFieldType)fieldNode["Type"].as<int>();
							field.UserModified = fieldNode["UserModified"] ? fieldNode["UserModified"].as<bool>() : false;
							switch (field.Type)
							{
							case LuaFieldType::Float:  field.FloatVal = fieldNode["Value"].as<float>();       break;
							case LuaFieldType::Int:    field.IntVal = fieldNode["Value"].as<int>();         break;
							case LuaFieldType::Bool:   field.BoolVal = fieldNode["Value"].as<bool>();        break;
							case LuaFieldType::String: field.StringVal = fieldNode["Value"].as<std::string>(); break;
							}
							sc.Fields[scriptPath].push_back(field);
						}
					}

					// Scrape any scripts that had no saved fields yet, or pick up new fields added to the script
						for (const auto& scriptPath : sc.ScriptPaths)
						{
							if (scriptPath.empty()) continue;

							std::filesystem::path fullPath = scriptPath;
							if (!std::filesystem::exists(fullPath))
								fullPath = std::filesystem::path("Assets") / scriptPath;
							if (!std::filesystem::exists(fullPath) && std::filesystem::exists("Assets"))
							{
								std::string searchName = std::filesystem::path(scriptPath).filename().string();
								if (searchName.find(".lua") == std::string::npos)
									searchName += ".lua";
								std::filesystem::path found = findScriptAnywhere(searchName);
								if (!found.empty())
									fullPath = found;
							}

							if (std::filesystem::exists(fullPath))
								LuaScriptEngine::ScrapeFieldsFromScript(fullPath, scriptPath, sc);
						}
				}

				auto lifetimeComponent = entity["LifetimeComponent"];
				if (lifetimeComponent)
				{
					auto& lc = deserializedEntity.AddComponent<LifetimeComponent>();
					lc.Lifetime = lifetimeComponent["Lifetime"].as<float>();
					lc.RemainingTime = lc.Lifetime;
				}

				auto transformComponent = entity["TransformComponent"];
				if (transformComponent)
				{
					auto& tc = deserializedEntity.GetComponent<TransformComponent>();
					tc.Translation = transformComponent["Translation"].as<glm::vec3>();
					tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();
					tc.Scale = transformComponent["Scale"].as<glm::vec3>();
				}

				auto cameraComponent = entity["CameraComponent"];
				if (cameraComponent && cameraComponent.IsMap()) {
					auto& cc = deserializedEntity.AddComponent<CameraComponent>();

					const auto& cameraProps = cameraComponent["Camera"];
					if (cameraProps)
					{
						if (cameraProps["ProjectionType"])
							cc.Camera.SetProjectionType(static_cast<SceneCamera::ProjectionType>(cameraProps["ProjectionType"].as<int>(1)));

						if (cameraProps["PerspectiveFov"])
							cc.Camera.SetPerspectiveVerticalFOV(cameraProps["PerspectiveFov"].as<float>(45.0f));
						if (cameraProps["PerspectiveNear"])
							cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>(0.01f));
						if (cameraProps["PerspectiveFar"])
							cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>(1000.0f));

						if (cameraProps["OrthographicSize"])
							cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>(10.0f));
						if (cameraProps["OrthographicNear"])
							cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>(-1.0f));
						if (cameraProps["OrthographicFar"])
							cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>(1.0f));
					}

					if (cameraComponent["Primary"])
						cc.Primary = cameraComponent["Primary"].as<bool>(true);
					if (cameraComponent["FixedAspectRatio"])
						cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>(false);

					// Fixed-aspect cameras never receive OnViewportResize, so
					// restore the persisted aspect (fall back to 16:9 for old
					// files that never stored one) or the projection is
					// degenerate (left == right == 0).
					if (cc.FixedAspectRatio)
					{
						float aspect = cameraComponent["AspectRatio"]
							? cameraComponent["AspectRatio"].as<float>(16.0f / 9.0f)
							: 16.0f / 9.0f;
						if (aspect > 0.0f)
							cc.Camera.SetAspectRatio(aspect);
					}

					if (cameraComponent["BackgroundColor"])
						cc.BackgroundColor = cameraComponent["BackgroundColor"].as<glm::vec4>();

					if (cameraComponent["BackgroundTilingFactor"])
						cc.BackgroundTilingFactor = cameraComponent["BackgroundTilingFactor"].as<glm::vec2>();

					if (cameraComponent["BackgroundFilterMode"])
						cc.BackgroundFilterMode = static_cast<TextureFilter>(cameraComponent["BackgroundFilterMode"].as<int>());

					if (cameraComponent["BackgroundImagePath"])
					{
						cc.BackgroundImagePath = cameraComponent["BackgroundImagePath"].as<std::string>();
						if (!cc.BackgroundImagePath.empty())
						{
							std::filesystem::path resolved = ResolveTexturePath(cc.BackgroundImagePath);
							if (std::filesystem::exists(resolved))
							{
								cc.BackgroundImage = Texture2D::Create(resolved.string(), cc.BackgroundFilterMode);
							}
						}
					}
				}

				auto spriteRendererComponent = entity["SpriteRendererComponent"];
				if (spriteRendererComponent)
				{
					auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
					src.Color = spriteRendererComponent["Color"].as<glm::vec4>();

					if (spriteRendererComponent["TexturePath"])
					{
						std::string texturePath = spriteRendererComponent["TexturePath"].as<std::string>();
						std::filesystem::path p(texturePath);
						std::string ext = p.extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
						// stbi (and the editor) accept all of these - a
						// narrower whitelist here silently dropped the path on
						// the next save (TexturePath is only written when the
						// texture loaded).
						if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
					    {
							std::filesystem::path resolved = ResolveTexturePath(texturePath);
							src.Texture = Texture2D::Create(resolved.string(), src.FilterMode);
						}
					}

					if (spriteRendererComponent["FilterMode"])
					{
						int filterModeValue = spriteRendererComponent["FilterMode"].as<int>();
						src.FilterMode = static_cast<Waffle::TextureFilter>(filterModeValue);

						if (src.Texture)
						{
							std::string currentTexturePath = src.Texture->GetPath();
							src.Texture = Texture2D::Create(currentTexturePath, src.FilterMode);
						}
					}

					if (spriteRendererComponent["TilingFactor"])
						src.TilingFactor = spriteRendererComponent["TilingFactor"].as<glm::vec2>();
					if (spriteRendererComponent["SortingLayer"])
						src.SortingLayer = spriteRendererComponent["SortingLayer"].as<int>(0);
					if (spriteRendererComponent["SortingOrder"])
						src.SortingOrder = spriteRendererComponent["SortingOrder"].as<int>(0);
				}

				auto circleRendererComponent = entity["CircleRendererComponent"];
				if (circleRendererComponent)
				{
					auto& crc = deserializedEntity.AddComponent<CircleRendererComponent>();
					crc.Color = circleRendererComponent["Color"].as<glm::vec4>();
					crc.Thickness = circleRendererComponent["Thickness"].as<float>();
					crc.Fade = circleRendererComponent["Fade"].as<float>();
					if (circleRendererComponent["SortingLayer"])
						crc.SortingLayer = circleRendererComponent["SortingLayer"].as<int>(0);
					if (circleRendererComponent["SortingOrder"])
						crc.SortingOrder = circleRendererComponent["SortingOrder"].as<int>(0);
				}

				auto rigidbody2DComponent = entity["Rigidbody2DComponent"];
				if (rigidbody2DComponent)
				{
					auto& rb2d = deserializedEntity.AddComponent<Rigidbody2DComponent>();
					rb2d.Type = Rigidbody2DBodyTypeFromString(rigidbody2DComponent["BodyType"].as<std::string>());
					rb2d.FixedRotation = rigidbody2DComponent["FixedRotation"].as<bool>();

					if (rigidbody2DComponent["Mass"])
						rb2d.Mass = rigidbody2DComponent["Mass"].as<float>();
				}

				auto boxCollider2DComponent = entity["BoxCollider2DComponent"];
				if (!boxCollider2DComponent)
					boxCollider2DComponent = entity["RectCollider2DComponent"]; // Backwards compatibility

				if (boxCollider2DComponent)
				{
					auto& bc2d = deserializedEntity.AddComponent<BoxCollider2DComponent>();
					bc2d.Offset = boxCollider2DComponent["Offset"].as<glm::vec2>();
					bc2d.Size = boxCollider2DComponent["Size"].as<glm::vec2>();
					bc2d.Density = boxCollider2DComponent["Density"].as<float>();
					bc2d.Friction = boxCollider2DComponent["Friction"].as<float>();
					bc2d.Restitution = boxCollider2DComponent["Restitution"].as<float>();
					bc2d.RestitutionThreshold = boxCollider2DComponent["RestitutionThreshold"].as<float>();
					bc2d.IsTrigger = boxCollider2DComponent["IsTrigger"] ? boxCollider2DComponent["IsTrigger"].as<bool>() : false;
				}

				auto circleCollider2DComponent = entity["CircleCollider2DComponent"];
				if (circleCollider2DComponent)
				{
					auto& cc2d = deserializedEntity.AddComponent<CircleCollider2DComponent>();
					cc2d.Offset = circleCollider2DComponent["Offset"].as<glm::vec2>();
					cc2d.Radius = circleCollider2DComponent["Radius"].as<float>();
					cc2d.Density = circleCollider2DComponent["Density"].as<float>();
					cc2d.Friction = circleCollider2DComponent["Friction"].as<float>();
					cc2d.Restitution = circleCollider2DComponent["Restitution"].as<float>();
					cc2d.RestitutionThreshold = circleCollider2DComponent["RestitutionThreshold"].as<float>();
					cc2d.IsTrigger = circleCollider2DComponent["IsTrigger"] ? circleCollider2DComponent["IsTrigger"].as<bool>() : false;
				}

				auto polygonCollider2DComponent = entity["PolygonCollider2DComponent"];
				if (polygonCollider2DComponent)
				{
					auto& pc2d = deserializedEntity.AddComponent<PolygonCollider2DComponent>();
					pc2d.Offset = polygonCollider2DComponent["Offset"].as<glm::vec2>();
					pc2d.Density = polygonCollider2DComponent["Density"].as<float>();
					pc2d.Friction = polygonCollider2DComponent["Friction"].as<float>();
					pc2d.Restitution = polygonCollider2DComponent["Restitution"].as<float>();
					pc2d.RestitutionThreshold = polygonCollider2DComponent["RestitutionThreshold"].as<float>();
					pc2d.IsTrigger = polygonCollider2DComponent["IsTrigger"] ? polygonCollider2DComponent["IsTrigger"].as<bool>() : false;
					auto vertices = polygonCollider2DComponent["Vertices"];
					if (vertices)
					{
						for (auto v : vertices)
							pc2d.Vertices.push_back(v.as<glm::vec2>());
					}
				}

				auto animatorComponent = entity["AnimatorComponent"];
				if (animatorComponent)
				{
					auto& animator = deserializedEntity.AddComponent<AnimatorComponent>();
					animator.CurrentClip = animatorComponent["CurrentClip"].as<std::string>("");

					auto clips = animatorComponent["Clips"];
					if (clips)
					{
						for (auto clipNode : clips)
						{
							AnimationClip clip;
							clip.Name = clipNode["Name"].as<std::string>("Default");
							clip.TexturePath = clipNode["TexturePath"].as<std::string>("");
							clip.Columns = clipNode["Columns"].as<int>(1);
							clip.Rows = clipNode["Rows"].as<int>(1);
							clip.StartFrame = clipNode["StartFrame"].as<int>(0);
							clip.EndFrame = clipNode["EndFrame"].as<int>(0);
							clip.FPS = clipNode["FPS"].as<float>(12.0f);
							clip.Loop = clipNode["Loop"].as<bool>(true);

							auto kfPaths = clipNode["KeyframeImagePaths"];
							if (kfPaths)
							{
								for (auto kf : kfPaths)
									clip.KeyframeImagePaths.push_back(kf.as<std::string>());
							}

							if (!clip.TexturePath.empty())
							{
								std::filesystem::path resolved = ResolveTexturePath(clip.TexturePath);
								if (std::filesystem::exists(resolved))
									clip.Texture = Texture2D::Create(resolved.string());
							}
							clip.RefreshSubTextures();
							animator.Clips[clip.Name] = clip;
						}
					}
				}
			}
			catch (const YAML::Exception& e)
			{
				WF_CORE_ERROR("Scene '{0}': skipping malformed entity entry: {1}",
					filepath, e.what());
			}
			catch (const std::exception& e)
			{
				WF_CORE_ERROR("Scene '{0}': skipping entity entry: {1}",
					filepath, e.what());
			}
			}
		}

		return true;
	}

	bool SceneSerializer::DeserializeRuntime(const std::string& filepath)
	{
		// Not implemented yet
		WF_CORE_ASSERT(false);
		return false;
	}

	bool SceneSerializer::SerializeEntityToPrefab(Entity entity, const std::string& filepath)
	{
		if (!entity) return false;

		// Depth-capped descendant collection - a hand-edited cycle in the
		// hierarchy must not hang serialization.
		std::vector<Entity> descendants;
		std::vector<UUID> stack{ entity.GetUUID() };
		int depth = 0;
		while (!stack.empty() && depth < 256)
		{
			UUID current = stack.back();
			stack.pop_back();
			Entity e = entity.GetScene()->GetEntityByUUID(current);
			if (!e || !e.HasComponent<RelationshipComponent>())
				continue;
			for (UUID childUUID : e.GetComponent<RelationshipComponent>().Children)
			{
				Entity child = entity.GetScene()->GetEntityByUUID(childUUID);
				if (child)
				{
					descendants.push_back(child);
					stack.push_back(childUUID);
				}
			}
			depth++;
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Prefab" << YAML::Value << entity.GetComponent<TagComponent>().Tag;
		out << YAML::Key << "Entity" << YAML::Value;
		SerializeEntity(out, entity);

		// Embed descendants so the hierarchy survives the round trip.
		if (!descendants.empty())
		{
			out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
			for (auto& child : descendants)
				SerializeEntity(out, child);
			out << YAML::EndSeq;
		}
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		fout << out.c_str();
		if (!fout)
		{
			WF_CORE_ERROR("Failed to write prefab file '{0}'", filepath);
			return false;
		}
		return true;
	}

	// Component parsing shared by the prefab root and its embedded children.
	// RelationshipComponent is intentionally NOT handled here - hierarchy is
	// rebuilt from the serialized UUIDs after all entities exist.
	static void DeserializePrefabComponents(Entity& deserializedEntity, const YAML::Node& entityNode)
	{
		auto transformComponent = entityNode["TransformComponent"];
		if (transformComponent)
		{
			auto& tc = deserializedEntity.GetComponent<TransformComponent>();
			tc.Translation = transformComponent["Translation"].as<glm::vec3>();
			tc.Rotation = transformComponent["Rotation"].as<glm::vec3>();
			tc.Scale = transformComponent["Scale"].as<glm::vec3>();
		}

		auto cameraComponent = entityNode["CameraComponent"];
		if (cameraComponent && cameraComponent.IsMap())
		{
			auto& cc = deserializedEntity.AddComponent<CameraComponent>();

			const auto& cameraProps = cameraComponent["Camera"];
			if (cameraProps)
			{
				if (cameraProps["ProjectionType"])
					cc.Camera.SetProjectionType(static_cast<SceneCamera::ProjectionType>(cameraProps["ProjectionType"].as<int>(1)));
				if (cameraProps["PerspectiveFov"])
					cc.Camera.SetPerspectiveVerticalFOV(cameraProps["PerspectiveFov"].as<float>(45.0f));
				if (cameraProps["PerspectiveNear"])
					cc.Camera.SetPerspectiveNearClip(cameraProps["PerspectiveNear"].as<float>(0.01f));
				if (cameraProps["PerspectiveFar"])
					cc.Camera.SetPerspectiveFarClip(cameraProps["PerspectiveFar"].as<float>(1000.0f));
				if (cameraProps["OrthographicSize"])
					cc.Camera.SetOrthographicSize(cameraProps["OrthographicSize"].as<float>(10.0f));
				if (cameraProps["OrthographicNear"])
					cc.Camera.SetOrthographicNearClip(cameraProps["OrthographicNear"].as<float>(-1.0f));
				if (cameraProps["OrthographicFar"])
					cc.Camera.SetOrthographicFarClip(cameraProps["OrthographicFar"].as<float>(1.0f));
			}

			if (cameraComponent["Primary"])
				cc.Primary = cameraComponent["Primary"].as<bool>(true);
			if (cameraComponent["FixedAspectRatio"])
				cc.FixedAspectRatio = cameraComponent["FixedAspectRatio"].as<bool>(false);
			if (cc.FixedAspectRatio)
			{
				float aspect = cameraComponent["AspectRatio"]
					? cameraComponent["AspectRatio"].as<float>(16.0f / 9.0f)
					: 16.0f / 9.0f;
				if (aspect > 0.0f)
					cc.Camera.SetAspectRatio(aspect);
			}
			if (cameraComponent["BackgroundColor"])
				cc.BackgroundColor = cameraComponent["BackgroundColor"].as<glm::vec4>();
			if (cameraComponent["BackgroundTilingFactor"])
				cc.BackgroundTilingFactor = cameraComponent["BackgroundTilingFactor"].as<glm::vec2>();
			if (cameraComponent["BackgroundFilterMode"])
				cc.BackgroundFilterMode = static_cast<TextureFilter>(cameraComponent["BackgroundFilterMode"].as<int>());
			if (cameraComponent["BackgroundImagePath"])
			{
				cc.BackgroundImagePath = cameraComponent["BackgroundImagePath"].as<std::string>();
				if (!cc.BackgroundImagePath.empty())
				{
					std::filesystem::path resolved = ResolveTexturePath(cc.BackgroundImagePath);
					if (std::filesystem::exists(resolved))
						cc.BackgroundImage = Texture2D::Create(resolved.string(), cc.BackgroundFilterMode);
				}
			}
		}

		auto spriteRendererComponent = entityNode["SpriteRendererComponent"];
		if (spriteRendererComponent)
		{
			auto& src = deserializedEntity.AddComponent<SpriteRendererComponent>();
			src.Color = spriteRendererComponent["Color"].as<glm::vec4>();

			if (spriteRendererComponent["TexturePath"])
			{
				std::string texturePath = spriteRendererComponent["TexturePath"].as<std::string>();
				std::filesystem::path p(texturePath);
				std::string ext = p.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")
				{
					std::filesystem::path resolved = ResolveTexturePath(texturePath);
					src.Texture = Texture2D::Create(resolved.string(), src.FilterMode);
				}
			}

			if (spriteRendererComponent["FilterMode"])
			{
				int filterModeValue = spriteRendererComponent["FilterMode"].as<int>();
				src.FilterMode = static_cast<Waffle::TextureFilter>(filterModeValue);
				if (src.Texture)
				{
					std::string currentTexturePath = src.Texture->GetPath();
					src.Texture = Texture2D::Create(currentTexturePath, src.FilterMode);
				}
			}

			if (spriteRendererComponent["TilingFactor"])
				src.TilingFactor = spriteRendererComponent["TilingFactor"].as<glm::vec2>();
			if (spriteRendererComponent["SortingLayer"])
				src.SortingLayer = spriteRendererComponent["SortingLayer"].as<int>(0);
			if (spriteRendererComponent["SortingOrder"])
				src.SortingOrder = spriteRendererComponent["SortingOrder"].as<int>(0);
		}

		auto circleRendererComponent = entityNode["CircleRendererComponent"];
		if (circleRendererComponent)
		{
			auto& crc = deserializedEntity.AddComponent<CircleRendererComponent>();
			crc.Color = circleRendererComponent["Color"].as<glm::vec4>();
			crc.Thickness = circleRendererComponent["Thickness"].as<float>();
			crc.Fade = circleRendererComponent["Fade"].as<float>();
			if (circleRendererComponent["SortingLayer"])
				crc.SortingLayer = circleRendererComponent["SortingLayer"].as<int>(0);
			if (circleRendererComponent["SortingOrder"])
				crc.SortingOrder = circleRendererComponent["SortingOrder"].as<int>(0);
		}

		auto rigidbody2DComponent = entityNode["Rigidbody2DComponent"];
		if (rigidbody2DComponent)
		{
			auto& rb2d = deserializedEntity.AddComponent<Rigidbody2DComponent>();
			rb2d.Type = Rigidbody2DBodyTypeFromString(rigidbody2DComponent["BodyType"].as<std::string>());
			rb2d.FixedRotation = rigidbody2DComponent["FixedRotation"].as<bool>();
			if (rigidbody2DComponent["Mass"])
				rb2d.Mass = rigidbody2DComponent["Mass"].as<float>();
		}

		auto boxCollider2DComponent = entityNode["BoxCollider2DComponent"];
		if (!boxCollider2DComponent)
			boxCollider2DComponent = entityNode["RectCollider2DComponent"]; // Backwards compatibility

		if (boxCollider2DComponent)
		{
			auto& bc2d = deserializedEntity.AddComponent<BoxCollider2DComponent>();
			bc2d.Offset = boxCollider2DComponent["Offset"].as<glm::vec2>();
			bc2d.Size = boxCollider2DComponent["Size"].as<glm::vec2>();
			bc2d.Density = boxCollider2DComponent["Density"].as<float>();
			bc2d.Friction = boxCollider2DComponent["Friction"].as<float>();
			bc2d.Restitution = boxCollider2DComponent["Restitution"].as<float>();
			bc2d.RestitutionThreshold = boxCollider2DComponent["RestitutionThreshold"].as<float>();
			bc2d.IsTrigger = boxCollider2DComponent["IsTrigger"] ? boxCollider2DComponent["IsTrigger"].as<bool>() : false;
		}

		auto circleCollider2DComponent = entityNode["CircleCollider2DComponent"];
		if (circleCollider2DComponent)
		{
			auto& cc2d = deserializedEntity.AddComponent<CircleCollider2DComponent>();
			cc2d.Offset = circleCollider2DComponent["Offset"].as<glm::vec2>();
			cc2d.Radius = circleCollider2DComponent["Radius"].as<float>();
			cc2d.Density = circleCollider2DComponent["Density"].as<float>();
			cc2d.Friction = circleCollider2DComponent["Friction"].as<float>();
			cc2d.Restitution = circleCollider2DComponent["Restitution"].as<float>();
			cc2d.RestitutionThreshold = circleCollider2DComponent["RestitutionThreshold"].as<float>();
			cc2d.IsTrigger = circleCollider2DComponent["IsTrigger"] ? circleCollider2DComponent["IsTrigger"].as<bool>() : false;
		}

		auto polygonCollider2DComponent = entityNode["PolygonCollider2DComponent"];
		if (polygonCollider2DComponent)
		{
			auto& pc2d = deserializedEntity.AddComponent<PolygonCollider2DComponent>();
			pc2d.Offset = polygonCollider2DComponent["Offset"].as<glm::vec2>();
			pc2d.Density = polygonCollider2DComponent["Density"].as<float>();
			pc2d.Friction = polygonCollider2DComponent["Friction"].as<float>();
			pc2d.Restitution = polygonCollider2DComponent["Restitution"].as<float>();
			pc2d.RestitutionThreshold = polygonCollider2DComponent["RestitutionThreshold"].as<float>();
			pc2d.IsTrigger = polygonCollider2DComponent["IsTrigger"] ? polygonCollider2DComponent["IsTrigger"].as<bool>() : false;
			auto vertices = polygonCollider2DComponent["Vertices"];
			if (vertices)
			{
				for (auto v : vertices)
					pc2d.Vertices.push_back(v.as<glm::vec2>());
			}
		}

		auto lifetimeComponent = entityNode["LifetimeComponent"];
		if (lifetimeComponent)
		{
			auto& lc = deserializedEntity.AddComponent<LifetimeComponent>();
			lc.Lifetime = lifetimeComponent["Lifetime"].as<float>();
			lc.RemainingTime = lc.Lifetime;
		}

		auto animatorComponent = entityNode["AnimatorComponent"];
		if (animatorComponent)
		{
			auto& animator = deserializedEntity.AddComponent<AnimatorComponent>();
			animator.CurrentClip = animatorComponent["CurrentClip"].as<std::string>("");

			auto clips = animatorComponent["Clips"];
			if (clips)
			{
				for (auto clipNode : clips)
				{
					AnimationClip clip;
					clip.Name = clipNode["Name"].as<std::string>("Default");
					clip.TexturePath = clipNode["TexturePath"].as<std::string>("");
					clip.Columns = clipNode["Columns"].as<int>(1);
					clip.Rows = clipNode["Rows"].as<int>(1);
					clip.StartFrame = clipNode["StartFrame"].as<int>(0);
					clip.EndFrame = clipNode["EndFrame"].as<int>(0);
					clip.FPS = clipNode["FPS"].as<float>(12.0f);
					clip.Loop = clipNode["Loop"].as<bool>(true);

					auto kfPaths = clipNode["KeyframeImagePaths"];
					if (kfPaths)
					{
						for (auto kf : kfPaths)
							clip.KeyframeImagePaths.push_back(kf.as<std::string>());
					}

					if (!clip.TexturePath.empty())
					{
						std::filesystem::path resolved = ResolveTexturePath(clip.TexturePath);
						if (std::filesystem::exists(resolved))
							clip.Texture = Texture2D::Create(resolved.string());
					}
					clip.RefreshSubTextures();
					animator.Clips[clip.Name] = clip;
				}
			}
		}

		auto scriptComponent = entityNode["ScriptComponent"];
		if (scriptComponent)
		{
			auto& sc = deserializedEntity.AddComponent<ScriptComponent>();
			if (scriptComponent["ClassName"])
				sc.ClassName = scriptComponent["ClassName"].as<std::string>();

			if (scriptComponent["ScriptPath"])
				sc.ScriptPaths.push_back(scriptComponent["ScriptPath"].as<std::string>());
			if (scriptComponent["ScriptPaths"])
			{
				for (auto pNode : scriptComponent["ScriptPaths"])
					sc.ScriptPaths.push_back(pNode.as<std::string>());
			}
			if (sc.ScriptPaths.empty() && !sc.ClassName.empty())
				sc.ScriptPaths.push_back(sc.ClassName);

			auto publicFields = scriptComponent["PublicFields"];
			if (publicFields)
			{
				for (auto fieldNode : publicFields)
				{
					LuaField field;
					std::string scriptPath = fieldNode["Script"].as<std::string>();
					field.Name = fieldNode["Name"].as<std::string>();
					field.Type = (LuaFieldType)fieldNode["Type"].as<int>();
					field.UserModified = fieldNode["UserModified"] ? fieldNode["UserModified"].as<bool>() : false;
					switch (field.Type)
					{
					case LuaFieldType::Float:  field.FloatVal = fieldNode["Value"].as<float>();       break;
					case LuaFieldType::Int:    field.IntVal = fieldNode["Value"].as<int>();         break;
					case LuaFieldType::Bool:   field.BoolVal = fieldNode["Value"].as<bool>();        break;
					case LuaFieldType::String: field.StringVal = fieldNode["Value"].as<std::string>(); break;
					}
					sc.Fields[scriptPath].push_back(field);
				}
			}
		}
	}

	Entity SceneSerializer::DeserializePrefabToEntity(Scene* scene, const std::string& filepath, float x, float y)
	{
		if (!scene) return {};

		std::string content = VFS::ReadFileAsString(filepath);
		if (content.empty())
		{
			WF_CORE_ERROR("Failed to load prefab file '{0}'", filepath);
			return {};
		}

		YAML::Node data;
		try
		{
			data = YAML::Load(content);
		}
		catch (const std::exception& e)
		{
			WF_CORE_ERROR("Failed to parse prefab file '{0}': {1}", filepath, e.what());
			return {};
		}

		// prefab-internal UUID -> spawned Entity (hierarchy is rebuilt
		// through this table after every entity exists).
		std::unordered_map<uint64_t, Entity> spawned;
		Entity root{};

		// One malformed entry must not leak a half-built entity into the
		// scene - destroy everything spawned so far on failure.
		try
		{
			auto entityNode = data["Entity"];
			if (!entityNode) return {};

			std::string name = "Entity";
			auto tagComponent = entityNode["TagComponent"];
			if (tagComponent)
				name = tagComponent["Tag"].as<std::string>();

			root = scene->CreateEntity(name);
			uint64_t rootUUID = entityNode["Entity"].as<uint64_t>(0);
			if (rootUUID)
				spawned[rootUUID] = root;

			DeserializePrefabComponents(root, entityNode);

			if (x != 0.0f || y != 0.0f)
			{
				auto& tc = root.GetComponent<TransformComponent>();
				tc.Translation.x = x;
				tc.Translation.y = y;
			}

			// Spawn embedded descendants with fresh UUIDs.
			std::vector<std::pair<uint64_t, YAML::Node>> childNodes;
			auto extraEntities = data["Entities"];
			if (extraEntities)
			{
				for (auto en : extraEntities)
				{
					std::string childName = "Entity";
					auto childTag = en["TagComponent"];
					if (childTag)
						childName = childTag["Tag"].as<std::string>();

					Entity child = scene->CreateEntity(childName);
					uint64_t childUUID = en["Entity"].as<uint64_t>(0);
					if (childUUID)
					{
						spawned[childUUID] = child;
						childNodes.push_back({ childUUID, en });
					}
					DeserializePrefabComponents(child, en);
				}
			}

			// Rebuild the hierarchy: parent each child to the remapped parent
			// of its serialized relationship. References to UUIDs outside the
			// prefab (e.g. the root's original parent) are dropped.
			for (auto& [childUUID, node] : childNodes)
			{
				auto relationship = node["RelationshipComponent"];
				if (!relationship)
					continue;
				uint64_t parentUUID = relationship["Parent"].as<uint64_t>(0);
				if (!parentUUID)
					continue;
				auto parentIt = spawned.find(parentUUID);
				if (parentIt == spawned.end())
					continue;
				scene->ParentEntity(spawned[childUUID], parentIt->second);
			}
		}
		catch (const std::exception& e)
		{
			WF_CORE_ERROR("Failed to deserialize prefab '{0}': {1}", filepath, e.what());
			for (auto& [uuid, ent] : spawned)
			{
				if (ent)
					scene->DestroyEntity(ent);
			}
			return {};
		}

		return root;
	}
}