using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace Waffle.Scripting.Internal;

/// <summary>Owns the loaded script assembly and all live instances (play instances per entity, plus lazily created editor gizmo instances); dispatch uses delegates cached at instantiation - no per-frame reflection.</summary>
internal static unsafe class ScriptRuntime
{
    private sealed class ScriptType
    {
        public required Type ClrType;
        public string? Name;
        public MethodInfo? OnStart;
        public MethodInfo? OnDestroy;
        public MethodInfo? OnEnable;
        public MethodInfo? OnDisable;
        public MethodInfo? OnDrawGizmos;
        public MethodInfo? OnUpdateNoArgs;
        public MethodInfo? OnUpdateDt;
        public MethodInfo? OnCollisionEnter;
        public MethodInfo? OnCollisionExit;
        public MethodInfo? OnTriggerEnter;
        public MethodInfo? OnTriggerExit;
        public readonly Dictionary<string, MethodInfo> UiMethods = new();
        public readonly Dictionary<string, FieldInfo> Fields = new();

        public FieldInfo? FindField(string name)
        {
            if (Fields.TryGetValue(name, out var found))
                return found;
            found = ClrType.GetField(name,
                BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
            Fields[name] = found!;
            return found;
        }

        public MethodInfo? FindUiMethod(string name)
        {
            if (UiMethods.TryGetValue(name, out var found))
                return found;
            var method = ClrType.GetMethod(name,
                BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance,
                null, Type.EmptyTypes, null)
                ?? ClrType.GetMethod(name,
                    BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance,
                    null, new[] { typeof(Entity) }, null);
            UiMethods[name] = method!;
            return method;
        }
    }

    private sealed class ScriptInstance
    {
        public required int Handle;
        public required uint EntityId;
        public required ScriptType Info;
        public required WaffleBehaviour Instance;
        public Action? Start;
        public Action? Destroy;
        public Action? Enable;
        public Action? Disable;
        public Action? DrawGizmos;
        public Action? UpdateNoArgs;
        public Action<float>? UpdateDt;
        public Action<Entity>? OnCollisionEnter;
        public Action<Entity>? OnCollisionExit;
        public Action<Entity>? OnTriggerEnter;
        public Action<Entity>? OnTriggerExit;
    }

    private static ScriptLoadContext? _context;
    private static DateTime _assemblyLoadTimeUtc;

    private static readonly Dictionary<string, ScriptType> _types = new(StringComparer.Ordinal);
    private static readonly Dictionary<int, ScriptInstance> _byHandle = new();
    private static readonly Dictionary<uint, List<ScriptInstance>> _playByEntity = new();
    private static readonly Dictionary<(uint Entity, string Type), ScriptInstance> _gizmoInstances = new();
    private static int _nextHandle = 1;

    /// <summary>Managed detail of the most recent failure, read by the host via LastError.</summary>
    private static string _lastError = string.Empty;

    // --- logging (called from the public Waffle.Log too) ---

    internal static void LogInfo(string message) => LogTo(NativeApi.LogInfo, message);
    internal static void LogWarn(string message) => LogTo(NativeApi.LogWarn, message);
    internal static void LogError(string message) => LogTo(NativeApi.LogError, message);

    private static void LogTo(delegate* unmanaged<byte*, void> fn, string message)
    {
        if (fn is null || message is null)
            return;
        byte[] buffer = NativeApi.Utf8(message);
        fixed (byte* p = buffer)
            fn(p);
        NativeApi.Release(buffer);
    }

    // Assembly loading

    private static bool EnsureAssemblyLoaded()
    {
        // The host gives us the assembly path via CompileScripts; we load whatever is on disk there.
        if (_context is not null)
            return true;

        string assemblyPath = CompilerService.LastOutputPath;
        if (assemblyPath.Length == 0 || !File.Exists(assemblyPath))
        {
            _lastError = "scripts assembly has not been compiled yet";
            return false;
        }

        try
        {
            _context = new ScriptLoadContext(assemblyPath);
            Assembly assembly = _context.LoadFromAssemblyPath(assemblyPath);
            _assemblyLoadTimeUtc = File.GetLastWriteTimeUtc(assemblyPath);
            ScanScriptTypes(assembly);
            return true;
        }
        catch (Exception e)
        {
            _lastError = "loading scripts assembly failed: " + e.Message;
            LogWarn("ScriptRuntime: " + _lastError);
            _context = null;
            return false;
        }
    }

    /// <summary>Drops and (re)loads the scripts assembly when it changed on disk.</summary>
    private static void ReloadIfAssemblyChanged()
    {
        string assemblyPath = CompilerService.LastOutputPath;
        if (assemblyPath.Length == 0 || !File.Exists(assemblyPath))
            return;

        DateTime diskTime = File.GetLastWriteTimeUtc(assemblyPath);
        if (_context is not null && diskTime <= _assemblyLoadTimeUtc)
            return;
        if (_playByEntity.Count > 0)
            return; // never swap under a running game

        UnloadAssembly();
        EnsureAssemblyLoaded();
    }

    private static void UnloadAssembly()
    {
        if (_context is null)
            return;
        try
        {
            _context.Unload();
        }
        catch (Exception e)
        {
            LogWarn("ScriptRuntime: assembly unload failed: " + e.Message);
        }
        _context = null;
        _types.Clear();
    }

    private static void ScanScriptTypes(Assembly assembly)
    {
        const BindingFlags flags =
            BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly;

        Type behaviourType = typeof(WaffleBehaviour);

        foreach (Type type in assembly.GetTypes())
        {
            if (!behaviourType.IsAssignableFrom(type) || type.IsAbstract)
                continue;

            ScriptType scriptType = new() { ClrType = type, Name = type.Name };
            scriptType.OnStart = GetMethod(type, "OnStart", flags, Type.EmptyTypes);
            scriptType.OnDestroy = GetMethod(type, "OnDestroy", flags, Type.EmptyTypes);
            scriptType.OnEnable = GetMethod(type, "OnEnable", flags, Type.EmptyTypes);
            scriptType.OnDisable = GetMethod(type, "OnDisable", flags, Type.EmptyTypes);
            scriptType.OnDrawGizmos = GetMethod(type, "OnDrawGizmos", flags, Type.EmptyTypes);
            scriptType.OnUpdateNoArgs = GetMethod(type, "OnUpdate", flags, Type.EmptyTypes);
            scriptType.OnUpdateDt = GetMethod(type, "OnUpdate", flags, new[] { typeof(float) });
            scriptType.OnCollisionEnter = GetMethod(type, "OnCollisionEnter2D", flags, new[] { typeof(Entity) })
                ?? GetMethod(type, "OnCollisionEnter2D", flags, Type.EmptyTypes);
            scriptType.OnCollisionExit = GetMethod(type, "OnCollisionExit2D", flags, new[] { typeof(Entity) })
                ?? GetMethod(type, "OnCollisionExit2D", flags, Type.EmptyTypes);
            scriptType.OnTriggerEnter = GetMethod(type, "OnTriggerEnter2D", flags, new[] { typeof(Entity) })
                ?? GetMethod(type, "OnTriggerEnter2D", flags, Type.EmptyTypes);
            scriptType.OnTriggerExit = GetMethod(type, "OnTriggerExit2D", flags, new[] { typeof(Entity) })
                ?? GetMethod(type, "OnTriggerExit2D", flags, Type.EmptyTypes);

            _types[type.Name] = scriptType;
        }
    }

    private static MethodInfo? GetMethod(Type type, string name, BindingFlags flags, Type[] parameters)
    {
        try
        {
            return type.GetMethod(name, flags, null, parameters, null);
        }
        catch (AmbiguousMatchException)
        {
            LogWarn($"ScriptRuntime: {type.Name}.{name} is ambiguous - pick one signature");
            return null;
        }
    }

    // Engine entry points

    public static void RuntimeStart()
    {
        // Fresh assembly every play (compile-on-play already ran host-side).
        UnloadAssembly();
        ClearPlayInstances();
        EnsureAssemblyLoaded();
    }

    public static void RuntimeStop()
    {
        // Host already fired OnDestroy per instance. Drop play + gizmo instances; PersistentData deliberately survives (Global table semantics).
        ClearPlayInstances();
        ClearGizmoInstances();
        UnloadAssembly();
    }

    public static int InstanceCreate(uint entityId, string typeName)
    {
        if (!EnsureAssemblyLoaded())
            return -1;
        if (!_types.TryGetValue(typeName, out ScriptType? type))
        {
            _lastError = $"no script class named '{typeName}' in the compiled assembly";
            return -1;
        }

        WaffleBehaviour instance;
        try
        {
            instance = (WaffleBehaviour)Activator.CreateInstance(type.ClrType)!;
        }
        catch (Exception e)
        {
            LogWarn($"ScriptRuntime: failed to construct {typeName}: {e.Message}");
            return -1;
        }

        int handle = _nextHandle++;
        ScriptInstance entry = new()
        {
            Handle = handle,
            EntityId = entityId,
            Info = type,
            Instance = instance,
        };
        BindMessages(entry);

        instance.Entity = new Entity(entityId);
        _byHandle[handle] = entry;
        if (!_playByEntity.TryGetValue(entityId, out var list))
        {
            list = new List<ScriptInstance>();
            _playByEntity[entityId] = list;
        }
        list.Add(entry);
        return handle;
    }

    public static void InstanceStart(int handle)
    {
        if (_byHandle.TryGetValue(handle, out var entry) && entry.Start is not null)
        {
            try
            {
                entry.Start();
            }
            catch (Exception e)
            {
                LogWarn($"{entry.Info.Name}.OnStart threw: {e}");
            }
        }
    }

    public static void InstanceSetField(int handle, string name, int kind,
        float f, float f2, int i, int b, string s)
    {
        if (!_byHandle.TryGetValue(handle, out var entry))
            return;
        FieldInfo? field = entry.Info.FindField(name);
        if (field is null || !field.FieldType.IsExplicitlyAssignableFromFieldValue(kind))
            return;
        try
        {
            switch (kind)
            {
                case ScriptFieldKind.Float: field.SetValue(entry.Instance, f); break;
                case ScriptFieldKind.Vec2: field.SetValue(entry.Instance, new System.Numerics.Vector2(f, f2)); break;
                case ScriptFieldKind.Int: field.SetValue(entry.Instance, i); break;
                case ScriptFieldKind.Bool: field.SetValue(entry.Instance, b != 0); break;
                case ScriptFieldKind.String: field.SetValue(entry.Instance, s); break;
            }
        }
        catch (Exception e)
        {
            LogWarn($"ScriptRuntime: setting {entry.Info.Name}.{name} failed: {e.Message}");
        }
    }

    public static bool InstanceGetField(int handle, string name, int kind,
        float* f, float* f2, int* i, int* b, byte* sBuf, int sBufLen)
    {
        if (!_byHandle.TryGetValue(handle, out var entry))
            return false;
        FieldInfo? field = entry.Info.FindField(name);
        if (field is null)
            return false;
        try
        {
            object? value = field.GetValue(entry.Instance);
            switch (kind)
            {
                case ScriptFieldKind.Float:
                    *f = Convert.ToSingle(value);
                    break;
                case ScriptFieldKind.Vec2:
                    var v = (System.Numerics.Vector2)value!;
                    *f = v.X;
                    *f2 = v.Y;
                    break;
                case ScriptFieldKind.Int:
                    *i = Convert.ToInt32(value);
                    break;
                case ScriptFieldKind.Bool:
                    *b = Convert.ToBoolean(value) ? 1 : 0;
                    break;
                case ScriptFieldKind.String:
                    string text = Convert.ToString(value) ?? string.Empty;
                    byte[] bytes = NativeApi.Utf8(text);
                    fixed (byte* p = bytes)
                    {
                        int copy = Math.Min(bytes.Length, sBufLen);
                        Buffer.MemoryCopy(p, sBuf, sBufLen, copy);
                    }
                    NativeApi.Release(bytes);
                    break;
            }
            return true;
        }
        catch (Exception e)
        {
            LogWarn($"ScriptRuntime: reading {entry.Info.Name}.{name} failed: {e.Message}");
            return false;
        }
    }

    public static void InstanceDestroy(int handle)
    {
        if (!_byHandle.TryGetValue(handle, out var entry))
            return;
        _byHandle.Remove(handle);
        if (_playByEntity.TryGetValue(entry.EntityId, out var list))
        {
            list.RemoveAll(i => i.Handle == handle);
            if (list.Count == 0)
                _playByEntity.Remove(entry.EntityId);
        }
        _gizmoInstances.Remove((entry.EntityId, entry.Info.Name ?? ""));

        if (entry.Destroy is not null)
        {
            try
            {
                entry.Destroy();
            }
            catch (Exception e)
            {
                LogWarn($"{entry.Info.Name}.OnDestroy threw: {e}");
            }
        }
    }

    public static void EntityUpdate(uint entityId, float dt)
    {
        if (!_playByEntity.TryGetValue(entityId, out var list))
            return;
        // Re-read Count each step: scripts may spawn/destroy entities, mutating this list mid-iteration.
        for (int i = 0; i < list.Count; i++)
        {
            ScriptInstance entry = list[i];
            if (entry.UpdateDt is not null)
            {
                try
                {
                    entry.UpdateDt(dt);
                }
                catch (Exception e)
                {
                    LogWarn($"{entry.Info.Name}.OnUpdate threw: {e}");
                }
            }
            else if (entry.UpdateNoArgs is not null)
            {
                try
                {
                    entry.UpdateNoArgs();
                }
                catch (Exception e)
                {
                    LogWarn($"{entry.Info.Name}.OnUpdate threw: {e}");
                }
            }
        }
    }

    public static void FireEnable(uint entityId, bool enabled)
    {
        if (!_playByEntity.TryGetValue(entityId, out var list))
            return;
        for (int i = 0; i < list.Count; i++)
        {
            ScriptInstance entry = list[i];
            Action? callback = enabled ? entry.Enable : entry.Disable;
            if (callback is null)
                continue;
            try
            {
                callback();
            }
            catch (Exception e)
            {
                LogWarn($"{entry.Info.Name}.{(enabled ? "OnEnable" : "OnDisable")} threw: {e}");
            }
        }
    }

    public static void CollisionEvent(uint selfId, uint otherId, int kind)
    {
        if (!_playByEntity.TryGetValue(selfId, out var list))
            return;
        Entity other = new(otherId);
        for (int i = 0; i < list.Count; i++)
        {
            ScriptInstance entry = list[i];
            Action<Entity>? callback;
            string name;
            switch (kind)
            {
                case ContactKind.CollisionBegin: (callback, name) = (entry.OnCollisionEnter, "OnCollisionEnter2D"); break;
                case ContactKind.CollisionEnd: (callback, name) = (entry.OnCollisionExit, "OnCollisionExit2D"); break;
                case ContactKind.TriggerBegin: (callback, name) = (entry.OnTriggerEnter, "OnTriggerEnter2D"); break;
                case ContactKind.TriggerEnd: (callback, name) = (entry.OnTriggerExit, "OnTriggerExit2D"); break;
                default: continue;
            }
            if (callback is null)
                continue;
            try
            {
                callback(other);
            }
            catch (Exception e)
            {
                LogWarn($"{entry.Info.Name}.{name} threw: {e}");
            }
        }
    }

    public static void UiHandler(string handlerName, uint buttonEntityId)
    {
        foreach (var pair in _playByEntity)
        {
            for (int i = 0; i < pair.Value.Count; i++)
            {
                ScriptInstance entry = pair.Value[i];
                MethodInfo? method = entry.Info.FindUiMethod(handlerName);
                if (method is null)
                    continue;
                try
                {
                    var parameters = method.GetParameters();
                    if (parameters.Length == 0)
                        method.Invoke(entry.Instance, null);
                    else
                        method.Invoke(entry.Instance, new object[] { new Entity(buttonEntityId) });
                }
                catch (Exception e)
                {
                    LogWarn($"{entry.Info.Name}.{handlerName} threw: {e.InnerException ?? e}");
                }
            }
        }
    }

    public static void EntityGizmos(uint entityId, string typeName)
    {
        if (!EnsureAssemblyLoaded())
            return;

        var key = (entityId, typeName);
        if (!_gizmoInstances.TryGetValue(key, out ScriptInstance? entry))
        {
            int handle = InstanceCreate(entityId, typeName);
            if (handle < 0)
                return;
            entry = _byHandle[handle];
            _gizmoInstances[key] = entry;
        }

        if (entry.DrawGizmos is null)
            return;
        try
        {
            entry.DrawGizmos();
        }
        catch (Exception e)
        {
            LogWarn($"{entry.Info.Name}.OnDrawGizmos threw: {e}");
        }
    }

    public static void GizmoInstancesClear()
    {
        ReloadIfAssemblyChanged();
        ClearGizmoInstances();
    }

    // Field scrape (editor inspector)

    public static int ScrapeFields(string typeName, ScriptFieldDef* outDefs, int maxDefs)
    {
        if (!EnsureAssemblyLoaded())
            return 0;
        if (!_types.TryGetValue(typeName, out ScriptType? type))
            return 0;

        object? probe = null;
        try
        {
            probe = Activator.CreateInstance(type.ClrType);
        }
        catch (Exception e)
        {
            LogWarn($"ScriptRuntime: cannot read default field values of {typeName} (constructor threw): {e.Message}");
        }

        const BindingFlags flags = BindingFlags.Public | BindingFlags.Instance;
        var fields = type.ClrType.GetFields(flags);
        int count = 0;
        foreach (FieldInfo field in fields)
        {
            if (count >= maxDefs)
                break;

            ScriptFieldDef def = default;
            WriteUtf8Field(def.Name, 64, field.Name);
            def.HasRange = 0;
            def.RangeMin = 0;
            def.RangeMax = 0;
            WriteUtf8Field(def.Tooltip, 128, string.Empty);

            var range = field.GetCustomAttribute<RangeAttribute>();
            if (range is not null)
            {
                def.HasRange = 1;
                def.RangeMin = range.Min;
                def.RangeMax = range.Max;
            }
            var tooltip = field.GetCustomAttribute<TooltipAttribute>();
            if (tooltip is not null)
                WriteUtf8Field(def.Tooltip, 128, tooltip.Text);

            object? value = probe is not null ? field.GetValue(probe) : null;
            if (field.FieldType == typeof(float))
            {
                def.Kind = ScriptFieldKind.Float;
                def.DefaultFloat = value is float f ? f : 0f;
            }
            else if (field.FieldType == typeof(int))
            {
                def.Kind = ScriptFieldKind.Int;
                def.DefaultInt = value is int i ? i : 0;
            }
            else if (field.FieldType == typeof(bool))
            {
                def.Kind = ScriptFieldKind.Bool;
                def.DefaultBool = value is bool b && b ? 1 : 0;
            }
            else if (field.FieldType == typeof(string))
            {
                def.Kind = ScriptFieldKind.String;
                WriteUtf8Field(def.DefaultString, 256, value as string ?? string.Empty);
            }
            else if (field.FieldType == typeof(System.Numerics.Vector2))
            {
                def.Kind = ScriptFieldKind.Vec2;
                var v = value is System.Numerics.Vector2 vec ? vec : default;
                def.DefaultFloat = v.X;
                def.DefaultFloat2 = v.Y;
            }
            else
            {
                continue; // unsupported inspector type
            }

            outDefs[count] = def;
            count++;
        }
        return count;
    }

    private static unsafe void WriteUtf8Field(byte* dest, int capacity, string text)
    {
        byte[] bytes = NativeApi.Utf8(text ?? string.Empty);
        int copy = Math.Min(bytes.Length, capacity);
        fixed (byte* src = bytes)
            Buffer.MemoryCopy(src, dest, capacity, copy);
        NativeApi.Release(bytes);
    }

    // Frame pump (timers)

    public static void Frame(float dt)
    {
        Time.Pump(dt);
    }

    // Helpers

    private static void BindMessages(ScriptInstance entry)
    {
        ScriptType type = entry.Info;
        try
        {
            if (type.OnStart is not null) entry.Start = type.OnStart.CreateDelegate<Action>(entry.Instance);
            if (type.OnDestroy is not null) entry.Destroy = type.OnDestroy.CreateDelegate<Action>(entry.Instance);
            if (type.OnEnable is not null) entry.Enable = type.OnEnable.CreateDelegate<Action>(entry.Instance);
            if (type.OnDisable is not null) entry.Disable = type.OnDisable.CreateDelegate<Action>(entry.Instance);
            if (type.OnDrawGizmos is not null) entry.DrawGizmos = type.OnDrawGizmos.CreateDelegate<Action>(entry.Instance);
            if (type.OnUpdateNoArgs is not null)
            {
                Action noArgs = type.OnUpdateNoArgs.CreateDelegate<Action>(entry.Instance);
                entry.UpdateNoArgs = noArgs;
            }
            else if (type.OnUpdateDt is not null)
            {
                entry.UpdateDt = type.OnUpdateDt.CreateDelegate<Action<float>>(entry.Instance);
            }
            if (type.OnCollisionEnter is not null) entry.OnCollisionEnter = BindEntityMessage(type.OnCollisionEnter, entry.Instance, "OnCollisionEnter2D");
            if (type.OnCollisionExit is not null) entry.OnCollisionExit = BindEntityMessage(type.OnCollisionExit, entry.Instance, "OnCollisionExit2D");
            if (type.OnTriggerEnter is not null) entry.OnTriggerEnter = BindEntityMessage(type.OnTriggerEnter, entry.Instance, "OnTriggerEnter2D");
            if (type.OnTriggerExit is not null) entry.OnTriggerExit = BindEntityMessage(type.OnTriggerExit, entry.Instance, "OnTriggerExit2D");
        }
        catch (Exception e)
        {
            LogWarn($"ScriptRuntime: bad message signature on {type.Name}: {e.Message}");
        }
    }

    private static Action<Entity>? BindEntityMessage(MethodInfo method, WaffleBehaviour instance, string messageName)
    {
        if (method.GetParameters().Length == 0)
        {
            // Parameterless variant: wrap, discard the other entity.
            Action noArgs = method.CreateDelegate<Action>(instance);
            return _ => noArgs();
        }
        return method.CreateDelegate<Action<Entity>>(instance);
    }

    private static void ClearPlayInstances()
    {
        // Safety net: fire OnDestroy for anything still registered (host normally does this per-entity).
        foreach (var pair in _byHandle)
        {
            if (pair.Value.Destroy is not null)
            {
                try
                {
                    pair.Value.Destroy();
                }
                catch { /* already logged per-instance elsewhere */ }
            }
        }
        _byHandle.Clear();
        _playByEntity.Clear();
    }

    private static void ClearGizmoInstances()
    {
        foreach (var entry in _gizmoInstances.Values)
        {
            _byHandle.Remove(entry.Handle);
            if (entry.Destroy is not null)
            {
                try
                {
                    entry.Destroy();
                }
                catch { }
            }
        }
        _gizmoInstances.Clear();
    }

    internal static void SetLastError(string message) => _lastError = message;
    internal static string GetLastError() => _lastError;
}

internal static class FieldKindExtensions
{
    /// <summary>True when the FieldInfo's runtime type matches the wire kind.</summary>
    public static bool IsExplicitlyAssignableFromFieldValue(this Type fieldType, int kind)
    {
        return kind switch
        {
            ScriptFieldKind.Float => fieldType == typeof(float),
            ScriptFieldKind.Vec2 => fieldType == typeof(System.Numerics.Vector2),
            ScriptFieldKind.Int => fieldType == typeof(int),
            ScriptFieldKind.Bool => fieldType == typeof(bool),
            ScriptFieldKind.String => fieldType == typeof(string),
            _ => false,
        };
    }
}
