#include "wfpch.h"
#include "Scene.h"

#include "Components.h"
#include "ScriptableEntity.h"
#include "Waffle/Scripting/LuaScriptEngine.h"
#include "Waffle/Renderer/Renderer2D.h"

#include <glm/glm.hpp>

#include "Entity.h"

// Box2D
#include "box2d/b2_world.h"
#include "box2d/b2_body.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_circle_shape.h"

namespace Waffle {

	static b2BodyType Rigid2DTypeToBox2DBody(Rigidbody2DComponent::BodyType bodyType)
	{
		switch (bodyType)
		{
			case Rigidbody2DComponent::BodyType::Static:	return b2BodyType::b2_staticBody;
			case Rigidbody2DComponent::BodyType::Dynamic:	return b2BodyType::b2_dynamicBody;
			case Rigidbody2DComponent::BodyType::Kinematic: return b2BodyType::b2_kinematicBody;
		}

		WF_CORE_ASSERT(false, "Unknown body type!");
		return b2_staticBody;
	}

	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
	}

	template<typename Component>
	static void CopyComponent(entt::registry& dst, entt::registry& src, const std::unordered_map<UUID, entt::entity>& enttMap)
	{
		auto view = src.view<Component>();
		for (auto e : view)
		{
			UUID uuid = src.get<IDComponent>(e).ID;
			WF_CORE_ASSERT(enttMap.find(uuid) != enttMap.end());
			entt::entity dstEnttID = enttMap.at(uuid);

			auto& component = src.get<Component>(e);
			dst.emplace_or_replace<Component>(dstEnttID, component);
		}
	}

	template<typename Component>
	static void CopyComponentIfExists(Entity dst, Entity src)
	{
		if (src.HasComponent<Component>())
			dst.AddOrReplaceComponent<Component>(src.GetComponent<Component>());
	}

	Ref<Scene> Scene::Copy(Ref<Scene> other)
	{
		Ref<Scene> newScene = CreateRef<Scene>();

		newScene->m_Name = other->m_Name;
		newScene->m_ViewportWidth = other->m_ViewportWidth;
		newScene->m_ViewportHeight = other->m_ViewportHeight;

		auto& srcSceneRegistry = other->m_Registry;
		auto& dstSceneRegistry = newScene->m_Registry;
		std::unordered_map<UUID, entt::entity> enttMap;

		// Create entities in new scene
		auto idView = srcSceneRegistry.view<IDComponent>();
		for (auto e : idView)
		{
			UUID uuid = srcSceneRegistry.get<IDComponent>(e).ID;
			const auto& name = srcSceneRegistry.get<TagComponent>(e).Tag;
			Entity newEntity = newScene->CreateEntityWithUUID(uuid, name);
			enttMap[uuid] = (entt::entity)newEntity;
		}

		// Copy components (except IDComponent and TagComponent)
		CopyComponent<RelationshipComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<ScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<LifetimeComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<TransformComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<SpriteRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleRendererComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CameraComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<NativeScriptComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<Rigidbody2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<BoxCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<CircleCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);
		CopyComponent<PolygonCollider2DComponent>(dstSceneRegistry, srcSceneRegistry, enttMap);

		return newScene;
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		return CreateEntityWithUUID(UUID(), name);
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		Entity entity = { m_Registry.create(), this };
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TransformComponent>();
		auto& tag = entity.AddComponent<TagComponent>();
		tag.Tag = name.empty() ? "Empty Entity" : name;
		return entity;
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		auto view = m_Registry.view<IDComponent>();
		for (auto entity : view)
		{
			if (view.get<IDComponent>(entity).ID == uuid)
			{
				return Entity(entity, this);
			}
		}
		return Entity();
	}

	void Scene::ParentEntity(Entity child, Entity parent)
	{
		if (!child || !parent || child == parent)
			return;

		// Cycle detection
		Entity currentParent = parent;
		while (currentParent && currentParent.HasComponent<RelationshipComponent>())
		{
			if (currentParent == child)
				return;
			UUID parentUUID = currentParent.GetComponent<RelationshipComponent>().Parent;
			currentParent = parentUUID != 0 ? GetEntityByUUID(parentUUID) : Entity{};
		}

		UnparentEntity(child);

		auto& childRel = child.HasComponent<RelationshipComponent>() ? child.GetComponent<RelationshipComponent>() : child.AddComponent<RelationshipComponent>();
		childRel.Parent = parent.GetUUID();

		auto& parentRel = parent.HasComponent<RelationshipComponent>() ? parent.GetComponent<RelationshipComponent>() : parent.AddComponent<RelationshipComponent>();
		parentRel.Children.push_back(child.GetUUID());
	}

	void Scene::UnparentEntity(Entity child)
	{
		if (!child || !child.HasComponent<RelationshipComponent>())
			return;

		auto& childRel = child.GetComponent<RelationshipComponent>();
		if (childRel.Parent != 0)
		{
			Entity parent = GetEntityByUUID(childRel.Parent);
			if (parent && parent.HasComponent<RelationshipComponent>())
			{
				auto& parentRel = parent.GetComponent<RelationshipComponent>();
				auto it = std::find(parentRel.Children.begin(), parentRel.Children.end(), child.GetUUID());
				if (it != parentRel.Children.end())
					parentRel.Children.erase(it);
			}
			childRel.Parent = 0;
		}
	}

	void Scene::DestroyEntity(Entity entity)
	{
		UnparentEntity(entity);
		if (entity.HasComponent<RelationshipComponent>())
		{
			auto children = entity.GetComponent<RelationshipComponent>().Children;
			for (auto childUUID : children)
			{
				Entity child = GetEntityByUUID(childUUID);
				if (child)
					UnparentEntity(child);
			}
		}

		m_Registry.destroy(entity);
	}

	void Scene::OnRuntimeStart()
	{
		m_PhysicsWorld = new b2World({ 0.0f, -9.8f });
		m_BodyEntityMap.clear();

		auto view = m_Registry.view<Rigidbody2DComponent>();
		for (auto e : view)
		{
			Entity entity = { e, this };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

			b2BodyDef bodyDef;
			bodyDef.type = Rigid2DTypeToBox2DBody(rb2d.Type);
			bodyDef.position.Set(transform.Translation.x, transform.Translation.y);
			bodyDef.angle = transform.Rotation.z;
			b2Body* body = m_PhysicsWorld->CreateBody(&bodyDef);
			body->SetFixedRotation(rb2d.FixedRotation);
			rb2d.RuntimeBody = body;

			m_BodyEntityMap[body] = (uint32_t)e;

			if (entity.HasComponent<BoxCollider2DComponent>())
			{
				auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();

				b2PolygonShape boxShape;
				boxShape.SetAsBox(bc2d.Size.x * transform.Scale.x, bc2d.Size.y * transform.Scale.y);

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &boxShape;
				fixtureDef.density = bc2d.Density;
				fixtureDef.friction = bc2d.Friction;
				fixtureDef.restitution = bc2d.Restitution;
				fixtureDef.restitutionThreshold = bc2d.RestitutionThreshold;
				body->CreateFixture(&fixtureDef);
			}

			if (entity.HasComponent<CircleCollider2DComponent>())
			{
				auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();

				b2CircleShape circleShape;
				circleShape.m_p.Set(cc2d.Offset.x, cc2d.Offset.y);
				circleShape.m_radius = transform.Scale.x * cc2d.Radius;

				b2FixtureDef fixtureDef;
				fixtureDef.shape = &circleShape;
				fixtureDef.density = cc2d.Density;
				fixtureDef.friction = cc2d.Friction;
				fixtureDef.restitution = cc2d.Restitution;
				fixtureDef.restitutionThreshold = cc2d.RestitutionThreshold;
				body->CreateFixture(&fixtureDef);
			}
		}

		LuaScriptEngine::OnRuntimeStart(this);
	}

	void Scene::OnRuntimeStop()
	{
		LuaScriptEngine::OnRuntimeStop(this);

		delete m_PhysicsWorld;
		m_PhysicsWorld = nullptr;
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
		if (!m_IsPaused || m_StepFrames-- > 0)
		{
			// Update scripts
			{
				LuaScriptEngine::OnRuntimeUpdate(this, ts);

				// TODO: MOVE TO SCENE ONSCENEPLAY WHEN YOU MAKE IT
				m_Registry.view<NativeScriptComponent>().each([=](auto entity, auto& nsc)
					{
						if (!nsc.Instance)
						{
							nsc.Instance = nsc.InstanciateScript();
							nsc.Instance->m_Entity = Entity{ entity, this };
							nsc.Instance->OnCreate();
						}

						nsc.Instance->OnUpdate(ts);
					});
			}

			// Physics
			{
				const int32_t velocityIterations = 6;
				const int32_t positionIterations = 2;
				m_PhysicsWorld->Step(ts, velocityIterations, positionIterations);

				// Retrieve transfrom from Box2D
				auto view = m_Registry.view<Rigidbody2DComponent>();
				for (auto e : view)
				{
					Entity entity = { e, this };
					auto& transform = entity.GetComponent<TransformComponent>();
					auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

					b2Body* body = (b2Body*)rb2d.RuntimeBody;
					const auto& position = body->GetPosition();
					transform.Translation.x = position.x;
					transform.Translation.y = position.y;
					transform.Rotation.z = body->GetAngle();
				}
			}
		}

		// Render 2D
		Camera* mainCamera = nullptr;
		glm::mat4 cameraTransform;
		CameraComponent* mainCameraComp = nullptr;
		{
			auto view = m_Registry.view<TransformComponent, CameraComponent>();
			for (auto entity : view)
			{
				auto components = view.get<TransformComponent, CameraComponent>(entity);
				auto [transform, camera] = components;

				if (camera.Primary)
				{
					mainCamera = &camera.Camera;
					cameraTransform = transform.GetTransform();
					mainCameraComp = &view.get<CameraComponent>(entity);
					break;
				}
			}
		}

		if (mainCamera)
		{
			Renderer2D::BeginScene(*mainCamera, cameraTransform);

			if (mainCameraComp && mainCameraComp->BackgroundImage)
			{
				float orthoSize = mainCameraComp->Camera.GetOrthographicSize();
				float aspectRatio = mainCameraComp->Camera.GetAspectRatio();
				glm::vec3 camPos = cameraTransform[3];
				glm::mat4 bgTransform = glm::translate(glm::mat4(1.0f), glm::vec3(camPos.x, camPos.y, -0.9f))
					* glm::scale(glm::mat4(1.0f), glm::vec3(orthoSize * aspectRatio * 2.0f, orthoSize * 2.0f, 1.0f));
				Renderer2D::DrawQuad(bgTransform, mainCameraComp->BackgroundImage, mainCameraComp->BackgroundTilingFactor);
			}

			// Draw sprites
			auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
			for (auto entity : group)
			{
				auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

				Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
			}

			// Draw circles
			auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
			for (auto entity : view)
			{
				auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);

				Renderer2D::DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade, (int)entity);
			}

			Renderer2D::EndScene();
		}
	}

	void Scene::OnUpdateEditor(Timestep ts, EditorCamera& camera)
	{
		Renderer2D::BeginScene(camera);

		Entity primaryCamEntity = GetPrimaryCameraEntity();
		if (primaryCamEntity && primaryCamEntity.HasComponent<CameraComponent>())
		{
			auto& camComp = primaryCamEntity.GetComponent<CameraComponent>();
			if (camComp.BackgroundImage)
			{
				const auto& transformComp = primaryCamEntity.GetComponent<TransformComponent>();
				float orthoSize = camComp.Camera.GetOrthographicSize();
				float aspectRatio = camComp.Camera.GetAspectRatio();
				glm::mat4 bgTransform = glm::translate(glm::mat4(1.0f), glm::vec3(transformComp.Translation.x, transformComp.Translation.y, -0.9f))
					* glm::scale(glm::mat4(1.0f), glm::vec3(orthoSize * aspectRatio * 2.0f, orthoSize * 2.0f, 1.0f));
				Renderer2D::DrawQuad(bgTransform, camComp.BackgroundImage, camComp.BackgroundTilingFactor);
			}
		}

		// Draw sprites
		auto group = m_Registry.group<TransformComponent>(entt::get<SpriteRendererComponent>);
		for (auto entity : group)
		{
			auto [transform, sprite] = group.get<TransformComponent, SpriteRendererComponent>(entity);

			//Renderer2D::DrawQuad(transform.GetTransform(), sprite.Color);
			Renderer2D::DrawSprite(transform.GetTransform(), sprite, (int)entity);
		}

		// Draw circles
		auto view = m_Registry.view<TransformComponent, CircleRendererComponent>();
		for (auto entity : view)
		{
			auto [transform, circle] = view.get<TransformComponent, CircleRendererComponent>(entity);

			Renderer2D::DrawCircle(transform.GetTransform(), circle.Color, circle.Thickness, circle.Fade, (int)entity);
		}

		Renderer2D::EndScene();
	}

	void Scene::Step(int frames)
	{
		m_StepFrames = frames;
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		m_ViewportWidth = width;
		m_ViewportHeight = height;

		// Resize the cameras that haven't locked their viewport aspect ratio
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
				cameraComponent.Camera.SetViewportSize(width, height);
		}
	}

	void Scene::DuplicateEntity(Entity entity)
	{
		std::string name = entity.GetName();
		Entity newEntity = CreateEntity(name);

		CopyComponentIfExists<RelationshipComponent>(newEntity, entity);
		CopyComponentIfExists<ScriptComponent>(newEntity, entity);
		CopyComponentIfExists<LifetimeComponent>(newEntity, entity);
		CopyComponentIfExists<TransformComponent>(newEntity, entity);
		CopyComponentIfExists<SpriteRendererComponent>(newEntity, entity);
		CopyComponentIfExists<CircleRendererComponent>(newEntity, entity);
		CopyComponentIfExists<CameraComponent>(newEntity, entity);
		CopyComponentIfExists<NativeScriptComponent>(newEntity, entity);
		CopyComponentIfExists<Rigidbody2DComponent>(newEntity, entity);
		CopyComponentIfExists<BoxCollider2DComponent>(newEntity, entity);
		CopyComponentIfExists<CircleCollider2DComponent>(newEntity, entity);
		CopyComponentIfExists<PolygonCollider2DComponent>(newEntity, entity);
	}

	Entity Scene::GetPrimaryCameraEntity()
	{
		auto view = m_Registry.view<CameraComponent>();
		for (auto entity : view)
		{
			const auto& camera = view.get<CameraComponent>(entity);
			if (camera.Primary)
				return Entity{ entity, this };
		}
		return {};
	}

	template<typename T>
	void Scene::OnComponentAdded(Entity entity, T& component)
	{
		static_assert(sizeof(T) != sizeof(T), "Unsupported component type in OnComponentAdded.");
	}

	template<>
	void Scene::OnComponentAdded<IDComponent>(Entity entity, IDComponent& component) {}

	template<>
	void Scene::OnComponentAdded<RelationshipComponent>(Entity entity, RelationshipComponent& component) {}

	template<>
	void Scene::OnComponentAdded<ScriptComponent>(Entity entity, ScriptComponent& component) {}

	template<>
	void Scene::OnComponentAdded<LifetimeComponent>(Entity entity, LifetimeComponent& component) {}

	template<>
	void Scene::OnComponentAdded<TransformComponent>(Entity entity, TransformComponent& component) {}

	template<>
	void Scene::OnComponentAdded<SpriteRendererComponent>(Entity entity, SpriteRendererComponent& component) {}

	template<>
	void Scene::OnComponentAdded<CircleRendererComponent>(Entity entity, CircleRendererComponent& component) {}

	template<>
	void Scene::OnComponentAdded<CameraComponent>(Entity entity, CameraComponent& component)
	{
		if (m_ViewportWidth > 0 && m_ViewportHeight > 0)
			component.Camera.SetViewportSize(m_ViewportWidth, m_ViewportHeight);
	}

	template<>
	void Scene::OnComponentAdded<TagComponent>(Entity entity, TagComponent& component) {}

	template<>
	void Scene::OnComponentAdded<NativeScriptComponent>(Entity entity, NativeScriptComponent& component) {}

	template<>
	void Scene::OnComponentAdded<Rigidbody2DComponent>(Entity entity, Rigidbody2DComponent& component) {}

	template<>
	void Scene::OnComponentAdded<BoxCollider2DComponent>(Entity entity, BoxCollider2DComponent& component) {}

	template<>
	void Scene::OnComponentAdded<CircleCollider2DComponent>(Entity entity, CircleCollider2DComponent& component) {}

	template<>
	void Scene::OnComponentAdded<PolygonCollider2DComponent>(Entity entity, PolygonCollider2DComponent& component) {}
}