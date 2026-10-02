using System.Runtime.InteropServices;
using System.Text;

namespace Waffle.Scripting.Internal;

/// <summary>Typed function pointers bound once at init from the host table; all script engine access routes through these - no marshaling, no hot-path allocations.</summary>
internal static unsafe class NativeApi
{
    // logging
    internal static delegate* unmanaged<byte*, void> LogInfo;
    internal static delegate* unmanaged<byte*, void> LogWarn;
    internal static delegate* unmanaged<byte*, void> LogError;

    // scene / framework
    internal static delegate* unmanaged<float*, float*, void> GetViewportSize;
    internal static delegate* unmanaged<float, float, float*, float*, void> ScreenToWorld;
    internal static delegate* unmanaged<int> EditorGizmoPass;

    // input
    internal static delegate* unmanaged<int, int> IsKeyPressed;
    internal static delegate* unmanaged<int, int> IsMouseButtonPressed;
    internal static delegate* unmanaged<int, int> IsKeyJustPressed;
    internal static delegate* unmanaged<int, int> IsKeyJustReleased;
    internal static delegate* unmanaged<int, int> IsMouseJustPressed;
    internal static delegate* unmanaged<int, int> IsMouseJustReleased;
    internal static delegate* unmanaged<float*, float*, void> GetMousePosition;
    internal static delegate* unmanaged<byte*, float> GetAxis;

    // scene management
    internal static delegate* unmanaged<int, void> ChangeScene;
    internal static delegate* unmanaged<int> GetCurrentSceneIndex;
    internal static delegate* unmanaged<int, void> SetCurrentSceneIndex;
    internal static delegate* unmanaged<void> RequestQuit;

    // entity management
    internal static delegate* unmanaged<byte*, float, float, int> CreateEntity;
    internal static delegate* unmanaged<uint, void> DestroyEntity;
    internal static delegate* unmanaged<uint, float, void> DestroyEntityDelayed;
    internal static delegate* unmanaged<uint, int> CloneEntity;
    internal static delegate* unmanaged<byte*, float, float, int> InstantiatePrefab;
    internal static delegate* unmanaged<uint, byte*, int, int> GetEntityName;
    internal static delegate* unmanaged<byte*, int> FindEntityByName;
    internal static delegate* unmanaged<byte*, uint*, int, int> FindAllEntitiesByName;
    internal static delegate* unmanaged<uint*, int, int> GetAllEntities;
    internal static delegate* unmanaged<uint, int> GetParent;
    internal static delegate* unmanaged<uint, uint*, int, int> GetChildren;
    internal static delegate* unmanaged<uint, uint, void> SetParent;
    internal static delegate* unmanaged<uint, void> Unparent;
    internal static delegate* unmanaged<uint, int, void> SetActive;
    internal static delegate* unmanaged<uint, int> IsActive;

    // transform
    internal static delegate* unmanaged<uint, float, float, float, void> Translate;
    internal static delegate* unmanaged<uint, float, float, float, void> SetPosition;
    internal static delegate* unmanaged<uint, float*, float*, float*, void> GetPosition;
    internal static delegate* unmanaged<uint, float, float, float, void> SetRotation;
    internal static delegate* unmanaged<uint, float, void> SetRotation2D;
    internal static delegate* unmanaged<uint, float*, float*, float*, void> GetRotation;
    internal static delegate* unmanaged<uint, float, float, float, void> SetScale;
    internal static delegate* unmanaged<uint, float*, float*, float*, void> GetScale;

    // physics 2D
    internal static delegate* unmanaged<uint, float, float, void> SetLinearVelocity;
    internal static delegate* unmanaged<uint, float*, float*, void> GetLinearVelocity;
    internal static delegate* unmanaged<uint, float, float, void> ApplyLinearImpulse;
    internal static delegate* unmanaged<uint, float, float, void> ApplyForce;
    internal static delegate* unmanaged<uint, float> GetAngularVelocity;
    internal static delegate* unmanaged<uint, float, void> SetAngularVelocity;
    internal static delegate* unmanaged<uint, float, void> ApplyTorque;
    internal static delegate* unmanaged<uint, float, void> ApplyAngularImpulse;
    internal static delegate* unmanaged<uint, int, void> SetSensor;
    internal static delegate* unmanaged<uint, int> IsSensor;
    internal static delegate* unmanaged<uint, float, void> SetGravityScale;
    internal static delegate* unmanaged<uint, float> GetGravityScale;
    internal static delegate* unmanaged<uint, int, void> SetFixedRotation;
    internal static delegate* unmanaged<uint, int> IsFixedRotation;
    internal static delegate* unmanaged<uint, float, void> SetFriction;
    internal static delegate* unmanaged<uint, float, void> SetRestitution;
    internal static delegate* unmanaged<uint, int, void> SetRigidBodyType;
    internal static delegate* unmanaged<uint, int> GetRigidBodyType;
    internal static delegate* unmanaged<uint, float, float, float, float, float,
        int*, float*, float*, float*, float*, int> Raycast;
    internal static delegate* unmanaged<float, float, float, uint, uint*, int, int> OverlapCircle;
    internal static delegate* unmanaged<float, float, float, float, uint, uint*, int, int> OverlapBox;

    // visual
    internal static delegate* unmanaged<uint, float, float, float, float, void> SetColor;
    internal static delegate* unmanaged<uint, float*, float*, float*, float*, void> GetColor;
    internal static delegate* unmanaged<uint, float, void> SetAlpha;
    internal static delegate* unmanaged<uint, byte*, void> SetTexture;

    // game UI
    internal static delegate* unmanaged<uint, byte*, void> SetUIText;
    internal static delegate* unmanaged<uint, byte*, int, int> GetUIText;
    internal static delegate* unmanaged<uint, float, void> SetUIProgress;
    internal static delegate* unmanaged<uint, byte*, void> SetUIImage;

    // animation
    internal static delegate* unmanaged<uint, byte*, void> PlayAnimation;
    internal static delegate* unmanaged<uint, void> StopAnimation;
    internal static delegate* unmanaged<uint, void> PauseAnimation;
    internal static delegate* unmanaged<uint, int, void> SetAnimationFrame;
    internal static delegate* unmanaged<uint, int> IsAnimationPlaying;

    // audio
    internal static delegate* unmanaged<byte*, float, float, int, int> PlaySound;
    internal static delegate* unmanaged<byte*, void> StopSound;
    internal static delegate* unmanaged<byte*, float, void> SetSoundVolume;
    internal static delegate* unmanaged<float, void> SetMasterVolume;

    // editor debug gizmos
    internal static delegate* unmanaged<float, float, float, float, float, float, float, float, float, void> GizmoDrawRay;
    internal static delegate* unmanaged<float, float, float, float, float, float, float, float, void> GizmoDrawLine;
    internal static delegate* unmanaged<float, float, float, float, float, float, float, void> GizmoDrawWireCircle;

    internal static void Bind(HostFunctions* fns)
    {
        LogInfo = (delegate* unmanaged<byte*, void>)fns->LogInfo;
        LogWarn = (delegate* unmanaged<byte*, void>)fns->LogWarn;
        LogError = (delegate* unmanaged<byte*, void>)fns->LogError;

        GetViewportSize = (delegate* unmanaged<float*, float*, void>)fns->GetViewportSize;
        ScreenToWorld = (delegate* unmanaged<float, float, float*, float*, void>)fns->ScreenToWorld;
        EditorGizmoPass = (delegate* unmanaged<int>)fns->EditorGizmoPass;

        IsKeyPressed = (delegate* unmanaged<int, int>)fns->IsKeyPressed;
        IsMouseButtonPressed = (delegate* unmanaged<int, int>)fns->IsMouseButtonPressed;
        IsKeyJustPressed = (delegate* unmanaged<int, int>)fns->IsKeyJustPressed;
        IsKeyJustReleased = (delegate* unmanaged<int, int>)fns->IsKeyJustReleased;
        IsMouseJustPressed = (delegate* unmanaged<int, int>)fns->IsMouseJustPressed;
        IsMouseJustReleased = (delegate* unmanaged<int, int>)fns->IsMouseJustReleased;
        GetMousePosition = (delegate* unmanaged<float*, float*, void>)fns->GetMousePosition;
        GetAxis = (delegate* unmanaged<byte*, float>)fns->GetAxis;

        ChangeScene = (delegate* unmanaged<int, void>)fns->ChangeScene;
        GetCurrentSceneIndex = (delegate* unmanaged<int>)fns->GetCurrentSceneIndex;
        SetCurrentSceneIndex = (delegate* unmanaged<int, void>)fns->SetCurrentSceneIndex;
        RequestQuit = (delegate* unmanaged<void>)fns->RequestQuit;

        CreateEntity = (delegate* unmanaged<byte*, float, float, int>)fns->CreateEntity;
        DestroyEntity = (delegate* unmanaged<uint, void>)fns->DestroyEntity;
        DestroyEntityDelayed = (delegate* unmanaged<uint, float, void>)fns->DestroyEntityDelayed;
        CloneEntity = (delegate* unmanaged<uint, int>)fns->CloneEntity;
        InstantiatePrefab = (delegate* unmanaged<byte*, float, float, int>)fns->InstantiatePrefab;
        GetEntityName = (delegate* unmanaged<uint, byte*, int, int>)fns->GetEntityName;
        FindEntityByName = (delegate* unmanaged<byte*, int>)fns->FindEntityByName;
        FindAllEntitiesByName = (delegate* unmanaged<byte*, uint*, int, int>)fns->FindAllEntitiesByName;
        GetAllEntities = (delegate* unmanaged<uint*, int, int>)fns->GetAllEntities;
        GetParent = (delegate* unmanaged<uint, int>)fns->GetParent;
        GetChildren = (delegate* unmanaged<uint, uint*, int, int>)fns->GetChildren;
        SetParent = (delegate* unmanaged<uint, uint, void>)fns->SetParent;
        Unparent = (delegate* unmanaged<uint, void>)fns->Unparent;
        SetActive = (delegate* unmanaged<uint, int, void>)fns->SetActive;
        IsActive = (delegate* unmanaged<uint, int>)fns->IsActive;

        Translate = (delegate* unmanaged<uint, float, float, float, void>)fns->Translate;
        SetPosition = (delegate* unmanaged<uint, float, float, float, void>)fns->SetPosition;
        GetPosition = (delegate* unmanaged<uint, float*, float*, float*, void>)fns->GetPosition;
        SetRotation = (delegate* unmanaged<uint, float, float, float, void>)fns->SetRotation;
        SetRotation2D = (delegate* unmanaged<uint, float, void>)fns->SetRotation2D;
        GetRotation = (delegate* unmanaged<uint, float*, float*, float*, void>)fns->GetRotation;
        SetScale = (delegate* unmanaged<uint, float, float, float, void>)fns->SetScale;
        GetScale = (delegate* unmanaged<uint, float*, float*, float*, void>)fns->GetScale;

        SetLinearVelocity = (delegate* unmanaged<uint, float, float, void>)fns->SetLinearVelocity;
        GetLinearVelocity = (delegate* unmanaged<uint, float*, float*, void>)fns->GetLinearVelocity;
        ApplyLinearImpulse = (delegate* unmanaged<uint, float, float, void>)fns->ApplyLinearImpulse;
        ApplyForce = (delegate* unmanaged<uint, float, float, void>)fns->ApplyForce;
        GetAngularVelocity = (delegate* unmanaged<uint, float>)fns->GetAngularVelocity;
        SetAngularVelocity = (delegate* unmanaged<uint, float, void>)fns->SetAngularVelocity;
        ApplyTorque = (delegate* unmanaged<uint, float, void>)fns->ApplyTorque;
        ApplyAngularImpulse = (delegate* unmanaged<uint, float, void>)fns->ApplyAngularImpulse;
        SetSensor = (delegate* unmanaged<uint, int, void>)fns->SetSensor;
        IsSensor = (delegate* unmanaged<uint, int>)fns->IsSensor;
        SetGravityScale = (delegate* unmanaged<uint, float, void>)fns->SetGravityScale;
        GetGravityScale = (delegate* unmanaged<uint, float>)fns->GetGravityScale;
        SetFixedRotation = (delegate* unmanaged<uint, int, void>)fns->SetFixedRotation;
        IsFixedRotation = (delegate* unmanaged<uint, int>)fns->IsFixedRotation;
        SetFriction = (delegate* unmanaged<uint, float, void>)fns->SetFriction;
        SetRestitution = (delegate* unmanaged<uint, float, void>)fns->SetRestitution;
        SetRigidBodyType = (delegate* unmanaged<uint, int, void>)fns->SetRigidBodyType;
        GetRigidBodyType = (delegate* unmanaged<uint, int>)fns->GetRigidBodyType;
        Raycast = (delegate* unmanaged<uint, float, float, float, float, float,
            int*, float*, float*, float*, float*, int>)fns->Raycast;
        OverlapCircle = (delegate* unmanaged<float, float, float, uint, uint*, int, int>)fns->OverlapCircle;
        OverlapBox = (delegate* unmanaged<float, float, float, float, uint, uint*, int, int>)fns->OverlapBox;

        SetColor = (delegate* unmanaged<uint, float, float, float, float, void>)fns->SetColor;
        GetColor = (delegate* unmanaged<uint, float*, float*, float*, float*, void>)fns->GetColor;
        SetAlpha = (delegate* unmanaged<uint, float, void>)fns->SetAlpha;
        SetTexture = (delegate* unmanaged<uint, byte*, void>)fns->SetTexture;

        SetUIText = (delegate* unmanaged<uint, byte*, void>)fns->SetUIText;
        GetUIText = (delegate* unmanaged<uint, byte*, int, int>)fns->GetUIText;
        SetUIProgress = (delegate* unmanaged<uint, float, void>)fns->SetUIProgress;
        SetUIImage = (delegate* unmanaged<uint, byte*, void>)fns->SetUIImage;

        PlayAnimation = (delegate* unmanaged<uint, byte*, void>)fns->PlayAnimation;
        StopAnimation = (delegate* unmanaged<uint, void>)fns->StopAnimation;
        PauseAnimation = (delegate* unmanaged<uint, void>)fns->PauseAnimation;
        SetAnimationFrame = (delegate* unmanaged<uint, int, void>)fns->SetAnimationFrame;
        IsAnimationPlaying = (delegate* unmanaged<uint, int>)fns->IsAnimationPlaying;

        PlaySound = (delegate* unmanaged<byte*, float, float, int, int>)fns->PlaySound;
        StopSound = (delegate* unmanaged<byte*, void>)fns->StopSound;
        SetSoundVolume = (delegate* unmanaged<byte*, float, void>)fns->SetSoundVolume;
        SetMasterVolume = (delegate* unmanaged<float, void>)fns->SetMasterVolume;

        GizmoDrawRay = (delegate* unmanaged<float, float, float, float, float, float, float, float, float, void>)fns->GizmoDrawRay;
        GizmoDrawLine = (delegate* unmanaged<float, float, float, float, float, float, float, float, void>)fns->GizmoDrawLine;
        GizmoDrawWireCircle = (delegate* unmanaged<float, float, float, float, float, float, float, void>)fns->GizmoDrawWireCircle;
    }

    // UTF-8 helpers

    /// <summary>Encodes a string as a null-terminated UTF-8 rented buffer.</summary>
    internal static byte[] Utf8(string message)
    {
        int max = Encoding.UTF8.GetMaxByteCount(message.Length) + 1;
        byte[] buffer = System.Buffers.ArrayPool<byte>.Shared.Rent(max);
        int written = Encoding.UTF8.GetBytes(message, buffer);
        buffer[written] = 0;
        return buffer;
    }

    internal static void Release(byte[] rented) => System.Buffers.ArrayPool<byte>.Shared.Return(rented);

    /// <summary>Reads a null-terminated UTF-8 string from a native buffer.</summary>
    internal static string ReadUtf8(byte* buffer, int maxLength)
    {
        int length = 0;
        while (length < maxLength && buffer[length] != 0)
            length++;
        return Encoding.UTF8.GetString(buffer, length);
    }
}
