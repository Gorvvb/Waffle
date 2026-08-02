#pragma once

#include "SceneCamera.h"
#include "Waffle/Core/UUID.h"
#include "Waffle/Renderer/Texture.h"

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Waffle {

	struct IDComponent
	{
		UUID ID;
		IDComponent() = default;
		IDComponent(const IDComponent&) = default;
		IDComponent(const UUID& uuid)
			: ID(uuid) {}
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(std::string& tag)
			: Tag(tag) {}
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;
		TransformComponent(glm::vec3& translation)
			: Translation(translation) {}

		glm::mat4 GetTransform() const
		{
			glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));
			return glm::translate(glm::mat4(1.0f), Translation)
				* rotation
				* glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct RelationshipComponent
	{
		UUID Parent = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent&) = default;
		RelationshipComponent(UUID parent)
			: Parent(parent) {}
	};

	enum class LuaFieldType { Float, Int, Bool, String };

	struct LuaField
	{
		std::string  Name;
		LuaFieldType Type = LuaFieldType::Float;
		float        FloatVal = 0.f;
		int          IntVal = 0;
		bool         BoolVal = false;
		std::string  StringVal;

		bool UserModified = false; // true = user changed this, don't overwrite from script
	};

	struct ScriptComponent
	{
		std::string              ClassName;
		std::vector<std::string> ScriptPaths;
		std::vector<std::string> ScriptTableKeys; // runtime only

		// Key = script path, value = public fields for that script
		std::unordered_map<std::string, std::vector<LuaField>> Fields;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent&) = default;
	};

	struct LifetimeComponent
	{
		float Lifetime = 5.0f;
		float RemainingTime = 5.0f;

		LifetimeComponent() = default;
		LifetimeComponent(const LifetimeComponent&) = default;
		LifetimeComponent(float lifetime)
			: Lifetime(lifetime), RemainingTime(lifetime) {}
	};

	struct SpriteRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		Ref<Texture2D> Texture;
		glm::vec2 TilingFactor = { 1.0f, 1.0f };

		TextureFilter FilterMode = TextureFilter::Linear;

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent&) = default;
		SpriteRendererComponent(const glm::vec4& color)
			: Color(color) {}
	};

	struct CircleRendererComponent
	{
		glm::vec4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Thickness = 1.0f;
		float Fade = 0.005f;

		CircleRendererComponent() = default;
		CircleRendererComponent(const CircleRendererComponent&) = default;
	};

	struct CameraComponent
	{
		Waffle::SceneCamera Camera;
		bool Primary = true;
		bool FixedAspectRatio = false;

		glm::vec4 BackgroundColor{ 0.1f, 0.1f, 0.1f, 1.0f };
		Ref<Texture2D> BackgroundImage = nullptr;
		std::string BackgroundImagePath = "";
		glm::vec2 BackgroundTilingFactor = { 1.0f, 1.0f };
		TextureFilter BackgroundFilterMode = TextureFilter::Linear;

		CameraComponent() = default;
		CameraComponent(const CameraComponent&) = default;
	};

	// TODO: Consider removing native scripting, since it is not used in the current engine architecture.
	// Forward declaration
	class ScriptableEntity;

	struct NativeScriptComponent
	{
		ScriptableEntity* Instance = nullptr;

		ScriptableEntity*(*InstanciateScript)();
		void (*DestroyScript)(NativeScriptComponent*);

		template<typename T>
		void Bind()
		{
			InstanciateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
			DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
		}
	};

	// Physics

	struct Rigidbody2DComponent
	{
		enum class BodyType { Static = 0, Dynamic, Kinematic };
		BodyType Type = BodyType::Static;
		bool FixedRotation = false;
		float Mass = 1.0f;

		// Storage for runtime
		void* RuntimeBody = nullptr;

		Rigidbody2DComponent() = default;
		Rigidbody2DComponent(const Rigidbody2DComponent&) = default;
	};

	struct BoxCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		glm::vec2 Size = { 0.5f, 0.5f };

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f; // Bounciness
		float RestitutionThreshold = 0.5f; // Threshold to stop endless bouncing

		bool IsTrigger = false; // When true, acts as a sensor (no collision response, fires OnTriggerBegin/End)

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		BoxCollider2DComponent() = default;
		BoxCollider2DComponent(const BoxCollider2DComponent&) = default;
	};

	struct CircleCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		float Radius = 0.5f;

		// TODO: Perhaps make it possible for the user to make a physics material, that will change these settnings.
		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		bool IsTrigger = false; // When true, acts as a sensor (no collision response, fires OnTriggerBegin/End)

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		CircleCollider2DComponent() = default;
		CircleCollider2DComponent(const CircleCollider2DComponent&) = default;
	};

	struct PolygonCollider2DComponent
	{
		glm::vec2 Offset = { 0.0f, 0.0f };
		std::vector<glm::vec2> Vertices;

		float Density = 1.0f;
		float Friction = 0.5f;
		float Restitution = 0.0f;
		float RestitutionThreshold = 0.5f;

		bool IsTrigger = false; // When true, acts as a sensor (no collision response, fires OnTriggerBegin/End)

		// Storage for runtime
		void* RuntimeFixture = nullptr;

		PolygonCollider2DComponent() = default;
		PolygonCollider2DComponent(const PolygonCollider2DComponent&) = default;
	};

	// Tag component that marks an entity as inactive.
	// When present, the entity is skipped by the script engine and renderer.
	struct DisabledComponent {};
}