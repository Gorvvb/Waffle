#include "TestHarness.h"

#include "Waffle/Scene/Components.h"
#include "Waffle/Scene/Scene.h"
#include "Waffle/Scene/Entity.h"
#include "Waffle/Scene/SceneSerializer.h"

#include <glm/glm.hpp>
#include <unordered_set>

using namespace Waffle;

namespace {

	// Builds one entity exercising every component that round-trips without asset files
	// (no textures/fonts - those need a renderer and a real asset tree).
	Entity MakeFullyLoadedEntity(Scene* scene, uint64_t uuid, const char* name)
	{
		Entity entity = scene->CreateEntityWithUUID(UUID(uuid), name);

		auto& transform = entity.GetComponent<TransformComponent>();
		transform.Translation = glm::vec3(1.5f, -2.0f, 0.25f);
		transform.Rotation = glm::vec3(0.0f, 0.0f, 0.5f);
		transform.Scale = glm::vec3(2.0f, 3.0f, 1.0f);

		auto& sprite = entity.AddComponent<SpriteRendererComponent>();
		sprite.Color = glm::vec4(1.0f, 0.5f, 0.25f, 1.0f);
		sprite.TilingFactor = glm::vec2(2.0f, 3.0f);
		sprite.SortingLayer = 2;
		sprite.SortingOrder = -7;
		sprite.AspectMode = SpriteAspectMode::Fit;

		auto& circle = entity.AddComponent<CircleRendererComponent>();
		circle.Color = glm::vec4(0.1f, 0.2f, 0.3f, 0.4f);
		circle.Thickness = 0.75f;
		circle.Fade = 0.125f;

		auto& camera = entity.AddComponent<CameraComponent>();
		camera.Primary = true;
		camera.FixedAspectRatio = true;
		camera.Camera.SetAspectRatio(1.75f);
		camera.BackgroundColor = glm::vec4(0.25f, 0.5f, 0.75f, 1.0f);
		camera.BackgroundTilingFactor = glm::vec2(3.0f, 4.0f);

		auto& rigidbody = entity.AddComponent<Rigidbody2DComponent>();
		rigidbody.Type = Rigidbody2DComponent::BodyType::Dynamic;
		rigidbody.FixedRotation = true;
		rigidbody.Mass = 3.5f;

		auto& box = entity.AddComponent<BoxCollider2DComponent>();
		box.Offset = glm::vec2(0.25f, -0.5f);
		box.Size = glm::vec2(1.5f, 2.5f);
		box.Density = 1.25f;
		box.Friction = 0.6f;
		box.Restitution = 0.3f;
		box.RestitutionThreshold = 0.5f;
		box.IsTrigger = true;

		auto& script = entity.AddComponent<ScriptComponent>();
		script.ScriptPaths.push_back("Tests/Player.cs");
		ScriptField floatField;
		floatField.Name = "Speed";
		floatField.Type = ScriptFieldType::Float;
		floatField.FloatVal = 1.5f;
		floatField.FloatVal2 = -2.0f;
		floatField.UserModified = true;
		script.Fields["Tests/Player.cs"].push_back(floatField);

		auto& lifetime = entity.AddComponent<LifetimeComponent>();
		lifetime.Lifetime = 2.5f;
		lifetime.RemainingTime = 2.5f;

		auto& particles = entity.AddComponent<ParticleSystemComponent>();
		particles.SpawnRate = 42.0f;
		particles.LifetimeMin = 0.25f;
		particles.LifetimeMax = 1.75f;
		particles.SpeedMin = 1.0f;
		particles.SpeedMax = 4.0f;
		particles.DirectionAngleDeg = 45.0f;
		particles.DirectionSpreadDeg = 90.0f;
		particles.EmissionRadius = 0.5f;
		particles.GravityY = -4.0f;
		particles.ColorStart = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
		particles.ColorEnd = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		particles.SizeStart = 0.4f;
		particles.SizeEnd = 0.05f;
		particles.MaxParticles = 128;
		particles.SortingLayer = 3;
		particles.SortingOrder = 9;

		return entity;
	}

	std::string SerializeToString(Ref<Scene> scene, Entity entity)
	{
		return SceneSerializer::SerializeEntityToString(entity);
	}

} // namespace

WTEST(scene_entity_serialization_round_trips_losslessly)
{
	Ref<Scene> scene = CreateRef<Scene>();
	Entity original = MakeFullyLoadedEntity(scene.get(), 0xA1B2C3D4E5F60708ull, "Round Trip");

	std::string snapshot = SceneSerializer::SerializeEntityToString(original);
	EXPECT_TRUE(!snapshot.empty(), "snapshot must not be empty");

	// Restore into a SECOND scene: the restored entity must be field-identical.
	Ref<Scene> restoredScene = CreateRef<Scene>();
	Entity restored = SceneSerializer::RestoreEntityFromSnapshot(restoredScene.get(), snapshot);
	EXPECT_TRUE(restored, "restored entity must be valid");
	EXPECT_TRUE((uint64_t)restored.GetUUID() == 0xA1B2C3D4E5F60708ull, "UUID must survive the round trip");

	std::string reserialized = SceneSerializer::SerializeEntityToString(restored);
	EXPECT_TRUE(snapshot == reserialized, "re-serialized snapshot must be byte-identical");

	// Spot-check a few fields survived (in case the string comparison ever loosens).
	const auto& transform = restored.GetComponent<TransformComponent>();
	EXPECT_NEAR(transform.Translation.x, 1.5, 1e-6);
	EXPECT_NEAR(transform.Rotation.z, 0.5, 1e-6);
	const auto& box = restored.GetComponent<BoxCollider2DComponent>();
	EXPECT_NEAR(box.Restitution, 0.3, 1e-6);
	EXPECT_TRUE(box.IsTrigger, "bool fields must round-trip");
	const auto& script = restored.GetComponent<ScriptComponent>();
	EXPECT_TRUE(script.ScriptPaths.size() == 1 && script.ScriptPaths[0] == "Tests/Player.cs",
		"script paths must round-trip");
	const auto& savedFields = script.Fields.at("Tests/Player.cs");
	EXPECT_NEAR(savedFields[0].FloatVal, 1.5, 1e-6);
	EXPECT_TRUE(savedFields[0].UserModified, "UserModified flag must round-trip");
	const auto& particles = restored.GetComponent<ParticleSystemComponent>();
	EXPECT_NEAR(particles.SpawnRate, 42.0, 1e-6);
	EXPECT_NEAR(particles.DirectionSpreadDeg, 90.0, 1e-6);
}

WTEST(scene_restore_replaces_the_existing_entity_in_place)
{
	Ref<Scene> scene = CreateRef<Scene>();
	Entity original = MakeFullyLoadedEntity(scene.get(), 0x1122334455667788ull, "Original");

	std::string snapshot = SceneSerializer::SerializeEntityToString(original);

	// Mutate the entity after snapshotting: a new name proves the restore replaced it.
	original.GetComponent<TagComponent>().Tag = "Mutated";
	Entity restored = SceneSerializer::RestoreEntityFromSnapshot(scene.get(), snapshot);

	EXPECT_TRUE(restored, "restore must succeed");
	EXPECT_TRUE(restored.GetName() == "Original", "restore must bring back the snapshotted name");

	// Exactly ONE entity with that UUID may exist after the restore.
	int count = 0;
	for (auto e : scene->GetAllEntitiesWith<IDComponent>())
	{
		Entity entity{ e, scene.get() };
		if ((uint64_t)entity.GetUUID() == 0x1122334455667788ull)
			count++;
	}
	EXPECT_TRUE(count == 1, "restore must not duplicate the entity");
}

WTEST(scene_copy_preserves_every_component_and_the_hierarchy)
{
	Ref<Scene> scene = CreateRef<Scene>();
	Entity parent = MakeFullyLoadedEntity(scene.get(), 0xAAAAAAAAAAAAAAAAull, "Parent");
	Entity child = scene->CreateEntityWithUUID(UUID(0xBBBBBBBBBBBBBBBBull), "Child");
	child.AddComponent<SpriteRendererComponent>();
	scene->ParentEntity(child, parent);

	Ref<Scene> copy = Scene::Copy(scene);

	// Every entity UUID in the original must exist in the copy, and their serialized
	// forms must be identical (components preserved 1:1).
	std::unordered_set<uint64_t> uuids;
	for (auto e : scene->GetAllEntitiesWith<IDComponent>())
	{
		Entity entity{ e, scene.get() };
		uint64_t uuid = (uint64_t)entity.GetUUID();
		uuids.insert(uuid);

		Entity copied = copy->GetEntityByUUID(uuid);
		EXPECT_TRUE(copied, "copy must contain every entity UUID");
		if (!copied)
			continue;
		std::string originalYaml = SceneSerializer::SerializeEntityToString(entity);
		std::string copiedYaml = SceneSerializer::SerializeEntityToString(copied);
		EXPECT_TRUE(originalYaml == copiedYaml, "copied entity must serialize identically");
	}

	// The hierarchy must survive: child's parent link points at the copied parent.
	Entity copiedChild = copy->GetEntityByUUID(0xBBBBBBBBBBBBBBBBull);
	EXPECT_TRUE(copiedChild, "child must exist in the copy");
	if (copiedChild)
	{
		Entity copiedParent = copy->GetEntityByUUID(0xAAAAAAAAAAAAAAAAull);
		EXPECT_TRUE(copiedParent, "parent must exist in the copy");
		if (copiedParent)
		{
			const auto& children = copiedParent.GetComponent<RelationshipComponent>().Children;
			EXPECT_TRUE(children.size() == 1 && children[0] == (uint64_t)copiedChild.GetUUID(),
				"parent's children list must reference the copied child");
		}
	}
}

WTEST(create_entity_rejects_duplicate_and_zero_uuids)
{
	Ref<Scene> scene = CreateRef<Scene>();
	Entity first = scene->CreateEntityWithUUID(UUID(0xDEADBEEFDEADBEEFull), "First");

	// A second entity claiming the same UUID must NOT shadow the first:
	// it gets a fresh id instead, and both stay reachable.
	Entity second = scene->CreateEntityWithUUID(UUID(0xDEADBEEFDEADBEEFull), "Second");
	EXPECT_TRUE(second, "second entity must be created");
	EXPECT_TRUE((uint64_t)first.GetUUID() == 0xDEADBEEFDEADBEEFull, "first entity keeps its UUID");
	EXPECT_TRUE((uint64_t)second.GetUUID() != 0xDEADBEEFDEADBEEFull, "second entity must get a fresh UUID");

	Entity firstByUuid = scene->GetEntityByUUID(UUID(0xDEADBEEFDEADBEEFull));
	EXPECT_TRUE(firstByUuid && firstByUuid.GetName() == "First",
		"lookup by the original UUID must still resolve to the FIRST entity");

	// Zero is the reserved "no id" value - it must never be assigned to an entity.
	Entity zero = scene->CreateEntityWithUUID(UUID(0ull), "Zero");
	EXPECT_TRUE(zero, "zero-UUID entity must be created");
	EXPECT_TRUE((uint64_t)zero.GetUUID() != 0, "zero must be replaced by a generated id");

	// Destroying the first entity frees the UUID: it must be reusable afterwards.
	scene->DestroyEntity(first);
	Entity reused = scene->CreateEntityWithUUID(UUID(0xDEADBEEFDEADBEEFull), "Reused");
	EXPECT_TRUE((uint64_t)reused.GetUUID() == 0xDEADBEEFDEADBEEFull,
		"a freed UUID must be assignable again");
}
