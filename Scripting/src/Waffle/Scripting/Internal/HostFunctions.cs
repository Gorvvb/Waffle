using System.Runtime.InteropServices;

namespace Waffle.Scripting.Internal;

// Managed mirror of Waffle/src/Waffle/Scripting/CSharpScriptHost.h. Layout is ABI: keep field-for-field in sync with the C++ HostFunctions struct (append-only).

[StructLayout(LayoutKind.Sequential)]
internal unsafe struct HostFunctions
{
    // logging
    public IntPtr LogInfo;
    public IntPtr LogWarn;
    public IntPtr LogError;

    // scene / framework
    public IntPtr GetViewportSize;
    public IntPtr ScreenToWorld;
    public IntPtr EditorGizmoPass;

    // input
    public IntPtr IsKeyPressed;
    public IntPtr IsMouseButtonPressed;
    public IntPtr IsKeyJustPressed;
    public IntPtr IsKeyJustReleased;
    public IntPtr IsMouseJustPressed;
    public IntPtr IsMouseJustReleased;
    public IntPtr GetMousePosition;
    public IntPtr GetAxis;

    // scene management
    public IntPtr ChangeScene;
    public IntPtr GetCurrentSceneIndex;
    public IntPtr SetCurrentSceneIndex;
    public IntPtr RequestQuit;

    // entity management
    public IntPtr CreateEntity;
    public IntPtr DestroyEntity;
    public IntPtr DestroyEntityDelayed;
    public IntPtr CloneEntity;
    public IntPtr InstantiatePrefab;
    public IntPtr GetEntityName;
    public IntPtr FindEntityByName;
    public IntPtr FindAllEntitiesByName;
    public IntPtr GetAllEntities;
    public IntPtr GetParent;
    public IntPtr GetChildren;
    public IntPtr SetParent;
    public IntPtr Unparent;
    public IntPtr SetActive;
    public IntPtr IsActive;

    // transform
    public IntPtr Translate;
    public IntPtr SetPosition;
    public IntPtr GetPosition;
    public IntPtr SetRotation;
    public IntPtr SetRotation2D;
    public IntPtr GetRotation;
    public IntPtr SetScale;
    public IntPtr GetScale;

    // physics 2D
    public IntPtr SetLinearVelocity;
    public IntPtr GetLinearVelocity;
    public IntPtr ApplyLinearImpulse;
    public IntPtr ApplyForce;
    public IntPtr GetAngularVelocity;
    public IntPtr SetAngularVelocity;
    public IntPtr ApplyTorque;
    public IntPtr ApplyAngularImpulse;
    public IntPtr SetSensor;
    public IntPtr IsSensor;
    public IntPtr SetGravityScale;
    public IntPtr GetGravityScale;
    public IntPtr SetFixedRotation;
    public IntPtr IsFixedRotation;
    public IntPtr SetFriction;
    public IntPtr SetRestitution;
    public IntPtr SetRigidBodyType;
    public IntPtr GetRigidBodyType;
    public IntPtr Raycast;
    public IntPtr OverlapCircle;
    public IntPtr OverlapBox;

    // visual
    public IntPtr SetColor;
    public IntPtr GetColor;
    public IntPtr SetAlpha;
    public IntPtr SetTexture;

    // game UI
    public IntPtr SetUIText;
    public IntPtr GetUIText;
    public IntPtr SetUIProgress;
    public IntPtr SetUIImage;

    // animation
    public IntPtr PlayAnimation;
    public IntPtr StopAnimation;
    public IntPtr PauseAnimation;
    public IntPtr SetAnimationFrame;
    public IntPtr IsAnimationPlaying;

    // audio
    public IntPtr PlaySound;
    public IntPtr StopSound;
    public IntPtr SetSoundVolume;
    public IntPtr SetMasterVolume;

    // editor debug gizmos
    public IntPtr GizmoDrawRay;
    public IntPtr GizmoDrawLine;
    public IntPtr GizmoDrawWireCircle;
}

/// <summary>Mirror of CSharpScriptHost::ScriptFieldKind.</summary>
internal static class ScriptFieldKind
{
    public const int Float = 0;
    public const int Int = 1;
    public const int Bool = 2;
    public const int String = 3;
    public const int Vec2 = 4;
}

/// <summary>Mirror of CSharpScriptHost::ContactKind.</summary>
internal static class ContactKind
{
    public const int CollisionBegin = 0;
    public const int CollisionEnd = 1;
    public const int TriggerBegin = 2;
    public const int TriggerEnd = 3;
}

/// <summary>Mirror of CSharpScriptHost::ScriptFieldDef (filled by ScrapeFields).</summary>
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct ScriptFieldDef
{
    public fixed byte Name[64];
    public int Kind;
    public float DefaultFloat;
    public float DefaultFloat2;
    public int DefaultInt;
    public int DefaultBool;
    public fixed byte DefaultString[256];
    public int HasRange;
    public float RangeMin;
    public float RangeMax;
    public fixed byte Tooltip[128];
}
