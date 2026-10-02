using System.Runtime.InteropServices;

namespace Waffle.Scripting.Internal;

/// <summary>Managed entry points the native host resolves via hostfxr. Names are ABI: CSharpScriptEngine.cpp resolves them by metadata name - don't rename without updating the C++ side.</summary>
internal static unsafe class WaffleNative
{
    [UnmanagedCallersOnly]
    public static int Init(HostFunctions* hostFunctions)
    {
        NativeApi.Bind(hostFunctions);
        return 0;
    }

    [UnmanagedCallersOnly]
    public static void Shutdown() { /* process-lifetime runtime; ALC unload happens at RuntimeStop */ }

    [UnmanagedCallersOnly]
    public static void Frame(float dt) => ScriptRuntime.Frame(dt);

    [UnmanagedCallersOnly]
    public static void RuntimeStart() => ScriptRuntime.RuntimeStart();

    [UnmanagedCallersOnly]
    public static void RuntimeStop() => ScriptRuntime.RuntimeStop();

    [UnmanagedCallersOnly]
    public static int InstanceCreate(uint entityId, byte* typeNameUtf8)
        => ScriptRuntime.InstanceCreate(entityId, NativeApi.ReadUtf8(typeNameUtf8, 256));

    [UnmanagedCallersOnly]
    public static void InstanceStart(int handle) => ScriptRuntime.InstanceStart(handle);

    [UnmanagedCallersOnly]
    public static void InstanceSetField(int handle, byte* nameUtf8, int kind,
        float f, float f2, int i, int b, byte* sUtf8)
        => ScriptRuntime.InstanceSetField(handle, NativeApi.ReadUtf8(nameUtf8, 256), kind,
            f, f2, i, b, NativeApi.ReadUtf8(sUtf8, 4096));

    [UnmanagedCallersOnly]
    public static int InstanceGetField(int handle, byte* nameUtf8, int kind,
        float* f, float* f2, int* i, int* b, byte* sBuf, int sBufLen)
        => ScriptRuntime.InstanceGetField(handle, NativeApi.ReadUtf8(nameUtf8, 256), kind,
            f, f2, i, b, sBuf, sBufLen) ? 1 : 0;

    [UnmanagedCallersOnly]
    public static void InstanceDestroy(int handle) => ScriptRuntime.InstanceDestroy(handle);

    [UnmanagedCallersOnly]
    public static void EntityUpdate(uint entityId, float dt) => ScriptRuntime.EntityUpdate(entityId, dt);

    [UnmanagedCallersOnly]
    public static void EntityGizmos(uint entityId, byte* typeNameUtf8)
        => ScriptRuntime.EntityGizmos(entityId, NativeApi.ReadUtf8(typeNameUtf8, 256));

    [UnmanagedCallersOnly]
    public static void GizmoInstancesClear() => ScriptRuntime.GizmoInstancesClear();

    [UnmanagedCallersOnly]
    public static void FireEnable(uint entityId, int enabled) => ScriptRuntime.FireEnable(entityId, enabled != 0);

    [UnmanagedCallersOnly]
    public static void CollisionEvent(uint selfId, uint otherId, int kind)
        => ScriptRuntime.CollisionEvent(selfId, otherId, kind);

    [UnmanagedCallersOnly]
    public static void UiHandler(byte* handlerNameUtf8, uint buttonEntityId)
        => ScriptRuntime.UiHandler(NativeApi.ReadUtf8(handlerNameUtf8, 256), buttonEntityId);

    [UnmanagedCallersOnly]
    public static int ScrapeFields(byte* typeNameUtf8, ScriptFieldDef* outDefs, int maxDefs)
        => ScriptRuntime.ScrapeFields(NativeApi.ReadUtf8(typeNameUtf8, 256), outDefs, maxDefs);

    [UnmanagedCallersOnly]
    public static int CompileScripts(byte* sourcesDirUtf8, byte* outputDllUtf8)
        => CompilerService.Compile(NativeApi.ReadUtf8(sourcesDirUtf8, 4096), NativeApi.ReadUtf8(outputDllUtf8, 4096));

    [UnmanagedCallersOnly]
    public static void SetScriptAssemblyPath(byte* assemblyPathUtf8)
        => CompilerService.SetLastOutputPath(NativeApi.ReadUtf8(assemblyPathUtf8, 4096));

    [UnmanagedCallersOnly]
    public static int LastError(byte* buffer, int bufferLen)
    {
        string message = ScriptRuntime.GetLastError();
        if (message.Length == 0)
            return 0;
        byte[] bytes = NativeApi.Utf8(message);
        fixed (byte* p = bytes)
        {
            int copy = Math.Min(bytes.Length, bufferLen);
            Buffer.MemoryCopy(p, buffer, bufferLen, copy);
        }
        NativeApi.Release(bytes);
        return Math.Min(message.Length, bufferLen);
    }
}
