#pragma once

#include "Waffle/Core/UUID.h"
#include "Waffle/Core/Timestep.h"
#include "Waffle/Renderer/EditorCamera.h"

#include "entt.hpp"

#include <glm/glm.hpp>

class b2World;
class b2Body;

#include "Waffle/Core/Ref.h"

namespace Waffle {

	class Entity;
	struct CameraComponent;

	class Scene : public RefCounted
	{
	private:
		std::string m_Name = "Untitled";
		entt::registry m_Registry;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		bool m_IsRunning = false;
		bool m_IsPaused = false;
		int m_StepFrames = 0;

		b2World* m_PhysicsWorld = nullptr;
		std::unordered_map<b2Body*, uint32_t> m_BodyEntityMap;
		std::unordered_map<UUID, entt::entity> m_EntityMap; // fast UUID -> entity lookup

		float m_GravityY = -9.8f;

		float m_PhysicsAccumulator = 0.0f;
		static constexpr float m_PhysicsFixedStep = 1.0f / 60.0f;

		friend class Entity;
		friend class SceneHierarchyPanel;
		friend class SceneSerializer;
		friend class CSharpScriptEngine;
	public:
		Scene();
		~Scene();

		static Ref<Scene> Copy(Ref<Scene> other);

		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		Entity GetEntityByUUID(UUID uuid);
		void DestroyEntity(Entity entity);

		void OnRuntimeStart();
		void OnRuntimeStop();

		// Creates the Box2D body/fixtures for an entity spawned mid-runtime; no-op otherwise.
		void CreateRuntimePhysicsBody(Entity entity);

		int OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts, EditorCamera& camera);
		void OnViewportResize(uint32_t width, uint32_t height);

		Entity DuplicateEntity(Entity entity);
		void ParentEntity(Entity child, Entity parent);
		void UnparentEntity(Entity child);

		// Hierarchy helpers
		Entity GetParent(Entity entity);
		// Returns the entity's transform in world space, composed with all ancestor transforms.
		glm::mat4 GetWorldTransform(Entity entity);

		uint32_t GetViewportWidth()  const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		void SetGravity(float g) { m_GravityY = g; }

		// Show/hide an entity (DisabledComponent - same state as SetActive); skips render/scripts/physics.
		void SetEntityHidden(Entity entity, bool hidden);

		Entity GetPrimaryCameraEntity();

		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; }

		bool IsRunning() const { return m_IsRunning; }
		bool IsPaused() const { return m_IsPaused; }

		void SetPaused(bool paused);

		void Step(int frames = 1);

		bool IsEntityValid(entt::entity entity) const { return m_Registry.valid(entity); }

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		b2World* GetPhysicsWorld() { return m_PhysicsWorld; }
		std::unordered_map<b2Body*, uint32_t>& GetBodyEntityMap() { return m_BodyEntityMap; }

		entt::registry& GetRegistry() { return m_Registry; }
	private:
		Entity DuplicateEntityRecursive(Entity entity, Entity parent);

		// Draws one tilemap's tiles (inside the sorted render pass, at its own sort slot).
		void DrawTilemapTiles(Entity entity, const glm::mat4& worldTransform);
		// Draws one particle system's live particles (inside the sorted render pass).
		void DrawParticles(entt::entity entity, const glm::mat4& worldTransform);

		// The single 2D render pass shared by the runtime and the editor viewport: gather, sort and draw.
		// Caller owns the camera, BeginScene/EndScene and any animator pre-pass.
		void GatherAndDrawRenderItems();
		// Draws the camera's background image (if it has one) stretched over the camera plane.
		void DrawCameraBackground(const CameraComponent& camComp, const glm::mat4& cameraTransform);

		template<typename T>
		void OnComponentAdded(Entity entity, T& component);
	};
}