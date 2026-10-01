using System.Numerics;
using Waffle.Scripting.Internal;

namespace Waffle;

/// <summary>
/// Lightweight handle to a scene entity. Blittable uint wrapper - entities are
/// never managed objects, so passing them around allocates nothing.
/// </summary>
public readonly struct Entity : IEquatable<Entity>
{
    internal readonly uint Id;

    internal Entity(uint id) => Id = id;

    /// <summary>The invalid entity, returned by failed lookups.</summary>
    public static Entity None => new(InvalidId);

    /// <summary>False for <see cref="None"/> (failed lookups, destroyed entities).</summary>
    public bool IsValid => Id != InvalidId;

    internal const uint InvalidId = uint.MaxValue;

    /// <summary>Display name (TagComponent) of the entity.</summary>
    public string Name => Interop.GetName(Id);

    /// <summary>World position (TransformComponent).</summary>
    public Vector3 Position
    {
        get => Interop.GetPosition(Id);
        set => Interop.SetPosition(Id, value);
    }

    /// <summary>Active state (DisabledComponent). Fires OnEnable/OnDisable on scripts.</summary>
    public bool Active
    {
        get => Interop.IsActive(Id);
        set => Interop.SetActive(Id, value);
    }

    /// <summary>Component accessor: <c>entity.Get&lt;Rigidbody2D&gt;()</c>.</summary>
    public T Get<T>()
    {
        if (typeof(T) == typeof(Rigidbody2D)) return (T)(object)new Rigidbody2D(this);
        if (typeof(T) == typeof(Transform2D)) return (T)(object)new Transform2D(this);
        if (typeof(T) == typeof(SpriteRenderer)) return (T)(object)new SpriteRenderer(this);
        if (typeof(T) == typeof(Animator2D)) return (T)(object)new Animator2D(this);
        if (typeof(T) == typeof(UIText)) return (T)(object)new UIText(this);
        throw new InvalidOperationException($"No component view for {typeof(T).Name}");
    }

    /// <summary>Destroys this entity (deferred to the end of the frame).</summary>
    public void Destroy() => Interop.DestroyEntity(Id);

    /// <summary>Destroys this entity after a delay in seconds.</summary>
    public void DestroyDelayed(float seconds) => Interop.DestroyEntityDelayed(Id, seconds);

    /// <summary>Full copy of this entity; returns the clone.</summary>
    public Entity Clone() => Interop.CloneEntity(Id);

    /// <summary>Parent entity, or <see cref="None"/>.</summary>
    public Entity Parent => Interop.GetParent(Id);

    /// <summary>All direct children.</summary>
    public System.Collections.Generic.List<Entity> Children => Interop.GetChildren(Id);

    /// <summary>Parents this entity under <paramref name="parent"/>.</summary>
    public void SetParent(Entity parent) => Interop.SetParent(Id, parent.Id);

    /// <summary>Detaches this entity from its parent.</summary>
    public void Unparent() => Interop.Unparent(Id);

    public bool Equals(Entity other) => Id == other.Id;
    public override bool Equals(object? obj) => obj is Entity e && Equals(e);
    public override int GetHashCode() => (int)Id;
    public override string ToString() => IsValid ? $"{Name} (Entity {Id})" : "Entity.None";

    public static bool operator ==(Entity a, Entity b) => a.Id == b.Id;
    public static bool operator !=(Entity a, Entity b) => a.Id != b.Id;

    public static implicit operator bool(Entity e) => e.IsValid;
}

/// <summary>Safe wrappers over the raw host pointers, shared by the public API.</summary>
internal static unsafe class Interop
{
    internal static string GetName(uint id)
    {
        byte[] buffer = new byte[256];
        fixed (byte* p = buffer)
        {
            int written = NativeApi.GetEntityName(id, p, buffer.Length);
            return written >= 0 ? NativeApi.ReadUtf8(p, buffer.Length) : string.Empty;
        }
    }

    internal static Vector3 GetPosition(uint id)
    {
        float x = 0, y = 0, z = 0;
        NativeApi.GetPosition(id, &x, &y, &z);
        return new Vector3(x, y, z);
    }

    internal static void SetPosition(uint id, Vector3 value)
        => NativeApi.SetPosition(id, value.X, value.Y, value.Z);

    internal static Vector3 GetRotation(uint id)
    {
        float x = 0, y = 0, z = 0;
        NativeApi.GetRotation(id, &x, &y, &z);
        return new Vector3(x, y, z);
    }

    internal static void SetRotation(uint id, Vector3 value)
        => NativeApi.SetRotation(id, value.X, value.Y, value.Z);

    internal static Vector3 GetScale(uint id)
    {
        float x = 1, y = 1, z = 1;
        NativeApi.GetScale(id, &x, &y, &z);
        return new Vector3(x, y, z);
    }

    internal static void SetScale(uint id, Vector3 value)
        => NativeApi.SetScale(id, value.X, value.Y, value.Z);

    internal static bool IsActive(uint id) => NativeApi.IsActive(id) != 0;

    internal static void SetActive(uint id, bool active) => NativeApi.SetActive(id, active ? 1 : 0);

    internal static void DestroyEntity(uint id) => NativeApi.DestroyEntity(id);

    internal static void DestroyEntityDelayed(uint id, float seconds) => NativeApi.DestroyEntityDelayed(id, seconds);

    internal static Entity CloneEntity(uint id)
    {
        int result = NativeApi.CloneEntity(id);
        return result >= 0 ? new Entity((uint)result) : Entity.None;
    }

    internal static Entity GetParent(uint id)
    {
        int result = NativeApi.GetParent(id);
        return result >= 0 ? new Entity((uint)result) : Entity.None;
    }

    internal static System.Collections.Generic.List<Entity> GetChildren(uint id)
    {
        Span<uint> ids = stackalloc uint[256];
        int count;
        fixed (uint* p = ids)
            count = NativeApi.GetChildren(id, p, ids.Length);
        var result = new System.Collections.Generic.List<Entity>(count);
        for (int i = 0; i < count; i++)
            result.Add(new Entity(ids[i]));
        return result;
    }

    internal static void SetParent(uint child, uint parent) => NativeApi.SetParent(child, parent);
    internal static void Unparent(uint child) => NativeApi.Unparent(child);
}
