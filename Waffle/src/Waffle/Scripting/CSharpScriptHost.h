#pragma once

// Native<->managed ABI contract: HostFunctions is a flat C fn-pointer table mirrored by the managed [StructLayout(Sequential)] struct - layout is ABI (append-only, update HostFunctions.cs in the same commit); strings are UTF-8, out-buffers caller-owned.

#include <cstdint>

namespace Waffle::CSharpScriptHost {

	// Mirror of Waffle.Scripting.Internal.ScriptFieldKind (managed).
	enum ScriptFieldKind : int
	{
		Field_Float = 0,
		Field_Int = 1,
		Field_Bool = 2,
		Field_String = 3,
		Field_Vec2 = 4,
	};

	// Mirror of Waffle.Scripting.Internal.ScriptFieldDef (managed); the managed ScrapeFields entry fills an array of these for the inspector.
	struct ScriptFieldDef
	{
		char Name[64];
		int  Kind;
		float DefaultFloat;
		float DefaultFloat2;   // Vec2 second component
		int   DefaultInt;
		int   DefaultBool;
		char  DefaultString[256];
		int   HasRange;
		float RangeMin;
		float RangeMax;
		char  Tooltip[128];
	};

	// Mirror of Waffle.Scripting.Internal.ContactKind (managed).
	enum ContactKind : int
	{
		Contact_CollisionBegin = 0,
		Contact_CollisionEnd = 1,
		Contact_TriggerBegin = 2,
		Contact_TriggerEnd = 3,
	};

	struct HostFunctions
	{
		// Logging
		void (*LogInfo)(const char* utf8);
		void (*LogWarn)(const char* utf8);
		void (*LogError)(const char* utf8);

		// Scene / framework queries
		void  (*GetViewportSize)(float* outW, float* outH);
		void  (*ScreenToWorld)(float sx, float sy, float* outWx, float* outWy);
		int   (*EditorGizmoPass)();               // managed gates mutating API with this

		// Input (codes are engine KeyCode/MouseCode values)
		int   (*IsKeyPressed)(int code);
		int   (*IsMouseButtonPressed)(int code);
		int   (*IsKeyJustPressed)(int code);
		int   (*IsKeyJustReleased)(int code);
		int   (*IsMouseJustPressed)(int code);
		int   (*IsMouseJustReleased)(int code);
		void  (*GetMousePosition)(float* outX, float* outY);   // viewport-relative
		float (*GetAxis)(const char* axisUtf8);

		// Scene management (deferred semantics live engine-side)
		void  (*ChangeScene)(int index);
		int   (*GetCurrentSceneIndex)();
		void  (*SetCurrentSceneIndex)(int index);
		void  (*RequestQuit)();

		// Entity management (ids are entt ids; -1 = invalid/none)
		int   (*CreateEntity)(const char* nameUtf8, float x, float y);
		void  (*DestroyEntity)(uint32_t entityId);             // deferred
		void  (*DestroyEntityDelayed)(uint32_t entityId, float delay);
		int   (*CloneEntity)(uint32_t entityId);
		int   (*InstantiatePrefab)(const char* pathUtf8, float x, float y);
		int   (*GetEntityName)(uint32_t entityId, char* buffer, int bufferSize); // returns -1 when missing
		int   (*FindEntityByName)(const char* nameUtf8);
		int   (*FindAllEntitiesByName)(const char* nameUtf8, uint32_t* outIds, int maxCount);
		int   (*GetAllEntities)(uint32_t* outIds, int maxCount);
		int   (*GetParent)(uint32_t entityId);
		int   (*GetChildren)(uint32_t entityId, uint32_t* outIds, int maxCount);
		void  (*SetParent)(uint32_t childId, uint32_t parentId);
		void  (*Unparent)(uint32_t childId);
		void  (*SetActive)(uint32_t entityId, int active);
		int   (*IsActive)(uint32_t entityId);

		// Transform
		void  (*Translate)(uint32_t entityId, float dx, float dy, float dz);
		void  (*SetPosition)(uint32_t entityId, float x, float y, float z);
		void  (*GetPosition)(uint32_t entityId, float* outX, float* outY, float* outZ);
		void  (*SetRotation)(uint32_t entityId, float x, float y, float z);
		void  (*SetRotation2D)(uint32_t entityId, float z);
		void  (*GetRotation)(uint32_t entityId, float* outX, float* outY, float* outZ);
		void  (*SetScale)(uint32_t entityId, float x, float y, float z);
		void  (*GetScale)(uint32_t entityId, float* outX, float* outY, float* outZ);

		// Physics 2D
		void  (*SetLinearVelocity)(uint32_t entityId, float vx, float vy);
		void  (*GetLinearVelocity)(uint32_t entityId, float* outVx, float* outVy);
		void  (*ApplyLinearImpulse)(uint32_t entityId, float ix, float iy);
		void  (*ApplyForce)(uint32_t entityId, float fx, float fy);
		float (*GetAngularVelocity)(uint32_t entityId);
		void  (*SetAngularVelocity)(uint32_t entityId, float omega);
		void  (*ApplyTorque)(uint32_t entityId, float torque);
		void  (*ApplyAngularImpulse)(uint32_t entityId, float impulse);
		void  (*SetSensor)(uint32_t entityId, int sensor);
		int   (*IsSensor)(uint32_t entityId);
		void  (*SetGravityScale)(uint32_t entityId, float scale);
		float (*GetGravityScale)(uint32_t entityId);
		void  (*SetFixedRotation)(uint32_t entityId, int fixedRotation);
		int   (*IsFixedRotation)(uint32_t entityId);
		void  (*SetFriction)(uint32_t entityId, float friction);
		void  (*SetRestitution)(uint32_t entityId, float restitution);
		void  (*SetRigidBodyType)(uint32_t entityId, int type); // 0 static, 1 kinematic, 2 dynamic
		int   (*GetRigidBodyType)(uint32_t entityId);
		int   (*Raycast)(uint32_t entityId, float offsetX, float offsetY, float dirX, float dirY, float distance,
			int32_t* outEntityId, float* outHitX, float* outHitY, float* outNormalX, float* outNormalY);
		int   (*OverlapCircle)(float cx, float cy, float radius, uint32_t excludeId, uint32_t* outIds, int maxCount);
		int   (*OverlapBox)(float cx, float cy, float halfW, float halfH, uint32_t excludeId, uint32_t* outIds, int maxCount);

		// Visual
		void  (*SetColor)(uint32_t entityId, float r, float g, float b, float a);
		void  (*GetColor)(uint32_t entityId, float* outR, float* outG, float* outB, float* outA);
		void  (*SetAlpha)(uint32_t entityId, float a);
		void  (*SetTexture)(uint32_t entityId, const char* pathUtf8);

		// Game UI
		void  (*SetUIText)(uint32_t entityId, const char* textUtf8);
		int   (*GetUIText)(uint32_t entityId, char* buffer, int bufferSize);
		void  (*SetUIProgress)(uint32_t entityId, float value);
		void  (*SetUIImage)(uint32_t entityId, const char* pathUtf8);

		// Animation
		void  (*PlayAnimation)(uint32_t entityId, const char* clipUtf8);
		void  (*StopAnimation)(uint32_t entityId);
		void  (*PauseAnimation)(uint32_t entityId);
		void  (*SetAnimationFrame)(uint32_t entityId, int frameIndex);
		int   (*IsAnimationPlaying)(uint32_t entityId);

		// Audio
		int   (*PlaySound)(const char* pathUtf8, float volume, float pitch, int loop);
		void  (*StopSound)(const char* pathUtf8);
		void  (*SetSoundVolume)(const char* pathUtf8, float volume);
		void  (*SetMasterVolume)(float volume);

		// Editor debug gizmos
		void  (*GizmoDrawRay)(float x, float y, float dx, float dy, float distance, float r, float g, float b, float a);
		void  (*GizmoDrawLine)(float x1, float y1, float x2, float y2, float r, float g, float b, float a);
		void  (*GizmoDrawWireCircle)(float x, float y, float radius, float r, float g, float b, float a);

		// Particles (append-only ABI: never reorder or remove - only add)
		void  (*ParticleBurst)(uint32_t entityId, int32_t count);
		void  (*ParticleSetEmitting)(uint32_t entityId, int32_t emitting);
		int32_t (*ParticleIsEmitting)(uint32_t entityId);
		int32_t (*ParticleAliveCount)(uint32_t entityId);

		// Render backend (append-only ABI: never reorder or remove - only add)
		void  (*GetRenderBackend)(char* buffer, int bufferSize);
		void  (*SetRenderBackend)(const char* backendUtf8);
	};

} // namespace Waffle::CSharpScriptHost
