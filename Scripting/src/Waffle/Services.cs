using System.Numerics;
using Waffle.Scripting.Internal;

namespace Waffle;

/// <summary>Keyboard, mouse and axis input. Editor-aware: input outside the game viewport or while typing in editor fields reports released.</summary>
public static unsafe class Input
{
    public static bool GetKey(KeyCode key) => NativeApi.IsKeyPressed((int)key) != 0;
    public static bool GetKeyDown(KeyCode key) => NativeApi.IsKeyJustPressed((int)key) != 0;
    public static bool GetKeyUp(KeyCode key) => NativeApi.IsKeyJustReleased((int)key) != 0;

    public static bool GetMouseButton(MouseButton button) => NativeApi.IsMouseButtonPressed((int)button) != 0;
    public static bool GetMouseButtonDown(MouseButton button) => NativeApi.IsMouseJustPressed((int)button) != 0;
    public static bool GetMouseButtonUp(MouseButton button) => NativeApi.IsMouseJustReleased((int)button) != 0;

    /// <summary>Mouse position relative to the game viewport.</summary>
    public static Vector2 MousePosition
    {
        get
        {
            float x = 0, y = 0;
            NativeApi.GetMousePosition(&x, &y);
            return new Vector2(x, y);
        }
    }

    /// <summary>Named axis value ("Horizontal", "Vertical"), -1..1.</summary>
    public static float GetAxis(string axis)
    {
        byte[] utf8 = NativeApi.Utf8(axis);
        fixed (byte* p = utf8)
        {
            float value = NativeApi.GetAxis(p);
            NativeApi.Release(utf8);
            return value;
        }
    }
}

/// <summary>Raycast and shape-overlap queries against the Box2D world.</summary>
public readonly struct RaycastResult
{
    public readonly Entity Entity;
    public readonly Vector2 Point;
    public readonly Vector2 Normal;

    internal RaycastResult(Entity entity, Vector2 point, Vector2 normal)
    {
        Entity = entity;
        Point = point;
        Normal = normal;
    }
}

public static unsafe class Physics2D
{
    /// <summary>Raycasts from an entity's position + offset along a direction; the entity's own colliders are ignored, false when nothing hit.</summary>
    public static bool Raycast(Entity from, Vector2 offset, Vector2 direction, float distance, out RaycastResult hit)
    {
        int hitEntity = -1;
        float hx = 0, hy = 0, nx = 0, ny = 0;
        int result = NativeApi.Raycast(from.Id, offset.X, offset.Y,
            direction.X, direction.Y, distance, &hitEntity, &hx, &hy, &nx, &ny);
        hit = new RaycastResult(
            hitEntity >= 0 ? new Entity((uint)hitEntity) : Entity.None,
            new Vector2(hx, hy), new Vector2(nx, ny));
        return result != 0;
    }

    /// <summary>All rigidbodies whose position lies within the circle.</summary>
    public static System.Collections.Generic.List<Entity> OverlapCircle(Vector2 center, float radius, Entity exclude = default)
    {
        uint excludeId = exclude.IsValid ? exclude.Id : uint.MaxValue;
        return QueryIds(Invoke);
        unsafe int Invoke(uint* buf, int max) => NativeApi.OverlapCircle(center.X, center.Y, radius, excludeId, buf, max);
    }

    /// <summary>All rigidbodies overlapping the axis-aligned box.</summary>
    public static System.Collections.Generic.List<Entity> OverlapBox(Vector2 center, Vector2 halfSize, Entity exclude = default)
    {
        uint excludeId = exclude.IsValid ? exclude.Id : uint.MaxValue;
        return QueryIds(Invoke);
        unsafe int Invoke(uint* buf, int max) => NativeApi.OverlapBox(center.X, center.Y, halfSize.X, halfSize.Y, excludeId, buf, max);
    }

    private unsafe delegate int QueryDelegate(uint* buffer, int maxCount);

    private static unsafe System.Collections.Generic.List<Entity> QueryIds(QueryDelegate query)
    {
        fixed (uint* ids = new uint[256])
        {
            int count = query(ids, 256);
            var result = new System.Collections.Generic.List<Entity>(count);
            for (int i = 0; i < count; i++)
                result.Add(new Entity(ids[i]));
            return result;
        }
    }
}

/// <summary>Scene queries: find entities by name, spawn, enumerate.</summary>
public static unsafe class Scene
{
    /// <summary>First entity named <paramref name="name"/>, or <see cref="Entity.None"/>.</summary>
    public static Entity FindByName(string name)
    {
        byte[] utf8 = NativeApi.Utf8(name);
        fixed (byte* p = utf8)
        {
            int id = NativeApi.FindEntityByName(p);
            NativeApi.Release(utf8);
            return id >= 0 ? new Entity((uint)id) : Entity.None;
        }
    }

    /// <summary>Every entity named <paramref name="name"/>.</summary>
    public static System.Collections.Generic.List<Entity> FindAllByName(string name)
    {
        byte[] utf8 = NativeApi.Utf8(name);
        fixed (byte* p = utf8)
        fixed (uint* ids = new uint[256])
        {
            int count = NativeApi.FindAllEntitiesByName(p, ids, 256);
            NativeApi.Release(utf8);
            return ToList(ids, count);
        }
    }

    /// <summary>Every entity in the scene.</summary>
    public static System.Collections.Generic.List<Entity> All()
    {
        fixed (uint* ids = new uint[1024])
        {
            int count = NativeApi.GetAllEntities(ids, 1024);
            return ToList(ids, count);
        }
    }

    /// <summary>Creates a new empty entity with a transform at position.</summary>
    public static Entity Create(string name = "Entity", Vector2 position = default)
    {
        byte[] utf8 = NativeApi.Utf8(name);
        fixed (byte* p = utf8)
        {
            int id = NativeApi.CreateEntity(p, position.X, position.Y);
            NativeApi.Release(utf8);
            return id >= 0 ? new Entity((uint)id) : Entity.None;
        }
    }

    /// <summary>Spawns a .prefab asset at a world position (scripts start immediately).</summary>
    public static Entity Instantiate(string prefabPath, Vector2 position = default)
    {
        byte[] utf8 = NativeApi.Utf8(prefabPath);
        fixed (byte* p = utf8)
        {
            int id = NativeApi.InstantiatePrefab(p, position.X, position.Y);
            NativeApi.Release(utf8);
            return id >= 0 ? new Entity((uint)id) : Entity.None;
        }
    }

    private static System.Collections.Generic.List<Entity> ToList(uint* ids, int count)
    {
        var result = new System.Collections.Generic.List<Entity>(count);
        for (int i = 0; i < count; i++)
            result.Add(new Entity(ids[i]));
        return result;
    }
}

/// <summary>Deferred scene switching (applied after the frame completes).</summary>
public static unsafe class SceneManager
{
    /// <summary>Queues a switch to the scene at the project's scene list index.</summary>
    public static void Load(int sceneIndex) => NativeApi.ChangeScene(sceneIndex);

    public static int CurrentSceneIndex => NativeApi.GetCurrentSceneIndex();
}

/// <summary>Frame timing and script-side timers.</summary>
public static unsafe class Time
{
    private static float _deltaTime;

    /// <summary>Seconds since the last frame.</summary>
    public static float DeltaTime => _deltaTime;

    /// <summary>Invokes <paramref name="callback"/> once after <paramref name="delay"/> seconds; cancel the returned handle to prevent it. Cleared on scene stop.</summary>
    public static TimerHandle SetTimer(float delay, Action callback)
    {
        if (Scripting.Internal.NativeApi.EditorGizmoPass() != 0)
        {
            Scripting.Internal.ScriptRuntime.LogWarn("Editor gizmo preview: SetTimer has no effect outside play mode");
            return default;
        }
        return TimerSystem.Schedule(delay, callback);
    }

    internal static void Pump(float dt)
    {
        _deltaTime = dt;
        TimerSystem.Pump(dt);
    }
}

/// <summary>Handle returned by <see cref="Time.SetTimer"/>.</summary>
public struct TimerHandle
{
    internal uint Id;
    internal bool Valid;

    public void Cancel() => TimerSystem.Cancel(Id);
}

internal static class TimerSystem
{
    private struct Entry
    {
        public uint Id;
        public float Remaining;
        public Action? Callback;
    }

    private static readonly List<Entry> _timers = new();
    private static uint _nextId = 1;

    internal static TimerHandle Schedule(float delay, Action callback)
    {
        uint id = _nextId++;
        _timers.Add(new Entry { Id = id, Remaining = delay, Callback = callback });
        return new TimerHandle { Id = id, Valid = true };
    }

    internal static void Cancel(uint id)
    {
        for (int i = 0; i < _timers.Count; i++)
        {
            if (_timers[i].Id == id)
            {
                // Index access only: a callback may Cancel itself or add timers.
                _timers[i] = new Entry { Id = _timers[i].Id, Remaining = _timers[i].Remaining, Callback = null };
                return;
            }
        }
    }

    internal static void Pump(float dt)
    {
        // Snapshot the count BEFORE invoking: callbacks may schedule or cancel timers, mutating this list mid-iteration.
        int count = _timers.Count;
        for (int i = 0; i < count && i < _timers.Count; i++)
        {
            Entry entry = _timers[i];
            entry.Remaining -= dt;
            _timers[i] = entry;
        }

        for (int i = 0; i < _timers.Count; i++)
        {
            if (_timers[i].Remaining > 0f)
                continue;
            Action? callback = _timers[i].Callback;
            _timers[i] = new Entry { Id = _timers[i].Id, Remaining = float.MaxValue, Callback = null };
            if (callback is not null)
            {
                try
                {
                    callback();
                }
                catch (Exception e)
                {
                    Scripting.Internal.ScriptRuntime.LogWarn($"Timer callback threw: {e}");
                }
            }
        }

        _timers.RemoveAll(t => t.Callback is null);
    }

    internal static void Clear() => _timers.Clear();
}

/// <summary>Sound playback via the engine audio system.</summary>
public static unsafe class Audio
{
    public static bool Play(string path, float volume = 1f, float pitch = 1f, bool loop = false)
    {
        byte[] utf8 = NativeApi.Utf8(path);
        fixed (byte* p = utf8)
        {
            bool ok = NativeApi.PlaySound(p, volume, pitch, loop ? 1 : 0) != 0;
            NativeApi.Release(utf8);
            return ok;
        }
    }

    /// <summary>Stops one sound by path, or every sound when null.</summary>
    public static void Stop(string? path = null)
    {
        if (path is null)
        {
            NativeApi.StopSound(null);
            return;
        }
        byte[] utf8 = NativeApi.Utf8(path);
        fixed (byte* p = utf8)
        {
            NativeApi.StopSound(p);
            NativeApi.Release(utf8);
        }
    }

    public static void SetVolume(string path, float volume)
    {
        byte[] utf8 = NativeApi.Utf8(path);
        fixed (byte* p = utf8)
        {
            NativeApi.SetSoundVolume(p, volume);
            NativeApi.Release(utf8);
        }
    }

    public static void SetMasterVolume(float volume) => NativeApi.SetMasterVolume(volume);
}

/// <summary>Editor debug drawing (visible while playing in the editor too; never drawn in exported games).</summary>
public static unsafe class Gizmos
{
    public static void DrawRay(Vector2 origin, Vector2 direction, float distance, Color? color = null)
    {
        var c = color ?? Color.Yellow;
        NativeApi.GizmoDrawRay(origin.X, origin.Y, direction.X, direction.Y, distance, c.R, c.G, c.B, c.A);
    }

    public static void DrawLine(Vector2 a, Vector2 b, Color? color = null)
    {
        var c = color ?? Color.Yellow;
        NativeApi.GizmoDrawLine(a.X, a.Y, b.X, b.Y, c.R, c.G, c.B, c.A);
    }

    public static void DrawWireCircle(Vector2 center, float radius, Color? color = null)
    {
        var c = color ?? Color.Yellow;
        NativeApi.GizmoDrawWireCircle(center.X, center.Y, radius, c.R, c.G, c.B, c.A);
    }
}

/// <summary>App-level queries and actions.</summary>
public static unsafe class Application
{
    /// <summary>Quits play mode in the editor; exits an exported game. Deferred to the end of the frame.</summary>
    public static void Quit() => NativeApi.RequestQuit();

    /// <summary>Current game viewport size in pixels.</summary>
    public static Vector2 ViewportSize
    {
        get
        {
            float w = 0, h = 0;
            NativeApi.GetViewportSize(&w, &h);
            return new Vector2(w, h);
        }
    }

    /// <summary>Converts a viewport-relative screen position to world space.</summary>
    public static Vector2 ScreenToWorld(Vector2 screen)
    {
        float wx = 0, wy = 0;
        NativeApi.ScreenToWorld(screen.X, screen.Y, &wx, &wy);
        return new Vector2(wx, wy);
    }
}

/// <summary>Render backend queries and switching. GetBackend returns the running backend; SetBackend persists the choice next to the executable and it applies on the NEXT launch (industry-standard restart-to-apply).</summary>
public static class Renderer
{
    /// <summary>The running render backend: "Vulkan" or "OpenGL".</summary>
    public static string GetBackend()
    {
        byte[] buffer = new byte[32];
        unsafe
        {
            fixed (byte* p = buffer)
                NativeApi.GetRenderBackend(p, buffer.Length);
        }
        int end = Array.IndexOf(buffer, (byte)0);
        return System.Text.Encoding.UTF8.GetString(buffer, 0, end < 0 ? 0 : end);
    }

    /// <summary>Requests a backend for the next launch ("Vulkan" or "OpenGL"). The current frame keeps rendering on the running backend.</summary>
    public static void SetBackend(string backend)
    {
        byte[] utf8 = System.Text.Encoding.UTF8.GetBytes(backend ?? "");
        unsafe
        {
            fixed (byte* p = utf8)
                NativeApi.SetRenderBackend(p);
        }
    }
}

/// <summary>Key/value store that survives scene changes AND script hot reloads - the replacement for the Lua engine's persistent Global table.</summary>
public static class PersistentData
{
    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, object> _data = new();

    public static void Set<T>(string key, T value) => _data[key] = value!;
    public static T Get<T>(string key, T fallback = default!)
        => _data.TryGetValue(key, out var value) && value is T typed ? typed : fallback;
    public static bool Has(string key) => _data.ContainsKey(key);
    public static bool Remove(string key) => _data.TryRemove(key, out _);
    public static void Clear() => _data.Clear();
}
