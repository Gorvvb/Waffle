using System.Numerics;
using Waffle.Scripting.Internal;

namespace Waffle;

// -------------------------------------------------------------------------
// Typed component views. Zero-allocation structs over the entity id - every
// property reads/writes the native component directly through the ABI.
// -------------------------------------------------------------------------

/// <summary>Box2D rigidbody view. Missing when the entity has no Rigidbody2DComponent.</summary>
public readonly unsafe struct Rigidbody2D
{
    private readonly Entity _entity;
    internal Rigidbody2D(Entity entity) => _entity = entity;

    public Vector2 Velocity
    {
        get
        {
            float vx = 0, vy = 0;
            NativeApi.GetLinearVelocity(_entity.Id, &vx, &vy);
            return new Vector2(vx, vy);
        }
        set => NativeApi.SetLinearVelocity(_entity.Id, value.X, value.Y);
    }

    public float AngularVelocity
    {
        get => NativeApi.GetAngularVelocity(_entity.Id);
        set => NativeApi.SetAngularVelocity(_entity.Id, value);
    }

    public float GravityScale
    {
        get => NativeApi.GetGravityScale(_entity.Id);
        set => NativeApi.SetGravityScale(_entity.Id, value);
    }

    public bool FixedRotation
    {
        get => NativeApi.IsFixedRotation(_entity.Id) != 0;
        set => NativeApi.SetFixedRotation(_entity.Id, value ? 1 : 0);
    }

    public bool IsSensor
    {
        get => NativeApi.IsSensor(_entity.Id) != 0;
        set => NativeApi.SetSensor(_entity.Id, value ? 1 : 0);
    }

    public BodyType BodyType
    {
        get => (BodyType)NativeApi.GetRigidBodyType(_entity.Id);
        set => NativeApi.SetRigidBodyType(_entity.Id, (int)value);
    }

    public void AddForce(Vector2 force) => NativeApi.ApplyForce(_entity.Id, force.X, force.Y);
    public void AddImpulse(Vector2 impulse) => NativeApi.ApplyLinearImpulse(_entity.Id, impulse.X, impulse.Y);
    public void AddTorque(float torque) => NativeApi.ApplyTorque(_entity.Id, torque);
    public void AddAngularImpulse(float impulse) => NativeApi.ApplyAngularImpulse(_entity.Id, impulse);

    public float Friction
    {
        set => NativeApi.SetFriction(_entity.Id, value);
    }

    public float Restitution
    {
        set => NativeApi.SetRestitution(_entity.Id, value);
    }
}

/// <summary>Transform view (position / rotation / scale, plus translate).</summary>
public readonly unsafe struct Transform2D
{
    private readonly Entity _entity;
    internal Transform2D(Entity entity) => _entity = entity;

    public Vector3 Position
    {
        get => _entity.Position;
        set => _entity.Position = value;
    }

    /// <summary>Z-only rotation in radians (the 2D workhorse).</summary>
    public float Rotation2D
    {
        get
        {
            Vector3 r = Interop.GetRotation(_entity.Id);
            return r.Z;
        }
        set => NativeApi.SetRotation2D(_entity.Id, value);
    }

    public Vector3 Rotation
    {
        get => Interop.GetRotation(_entity.Id);
        set => Interop.SetRotation(_entity.Id, value);
    }

    public Vector3 Scale
    {
        get => Interop.GetScale(_entity.Id);
        set => Interop.SetScale(_entity.Id, value);
    }

    public void Translate(Vector3 delta) => NativeApi.Translate(_entity.Id, delta.X, delta.Y, delta.Z);
}

/// <summary>Sprite view: color, alpha and runtime texture swapping.</summary>
public readonly unsafe struct SpriteRenderer
{
    private readonly Entity _entity;
    internal SpriteRenderer(Entity entity) => _entity = entity;

    public Color Color
    {
        get
        {
            float r = 1, g = 1, b = 1, a = 1;
            NativeApi.GetColor(_entity.Id, &r, &g, &b, &a);
            return new Color(r, g, b, a);
        }
        set => NativeApi.SetColor(_entity.Id, value.R, value.G, value.B, value.A);
    }

    public float Alpha
    {
        set => NativeApi.SetAlpha(_entity.Id, value);
    }

    /// <summary>Swaps the sprite texture from an asset-relative path.</summary>
    public void SetTexture(string path)
    {
        byte[] utf8 = NativeApi.Utf8(path);
        fixed (byte* p = utf8)
        {
            NativeApi.SetTexture(_entity.Id, p);
            NativeApi.Release(utf8);
        }
    }
}

/// <summary>Sprite animator view.</summary>
public readonly unsafe struct Animator2D
{
    private readonly Entity _entity;
    internal Animator2D(Entity entity) => _entity = entity;

    public bool IsPlaying => NativeApi.IsAnimationPlaying(_entity.Id) != 0;

    public void Play(string clipName)
    {
        byte[] utf8 = NativeApi.Utf8(clipName);
        fixed (byte* p = utf8)
        {
            NativeApi.PlayAnimation(_entity.Id, p);
            NativeApi.Release(utf8);
        }
    }

    public void Stop() => NativeApi.StopAnimation(_entity.Id);
    public void Pause() => NativeApi.PauseAnimation(_entity.Id);
    public void SetFrame(int index) => NativeApi.SetAnimationFrame(_entity.Id, index);
}

/// <summary>Game UI text view (UITextComponent).</summary>
public readonly unsafe struct UIText
{
    private readonly Entity _entity;
    internal UIText(Entity entity) => _entity = entity;

    public string Text
    {
        get
        {
            byte[] buffer = new byte[256];
            fixed (byte* p = buffer)
            {
                int written = NativeApi.GetUIText(_entity.Id, p, buffer.Length);
                return written >= 0 ? NativeApi.ReadUtf8(p, buffer.Length) : string.Empty;
            }
        }
        set
        {
            byte[] utf8 = NativeApi.Utf8(value);
            fixed (byte* p = utf8)
            {
                NativeApi.SetUIText(_entity.Id, p);
                NativeApi.Release(utf8);
            }
        }
    }
}

/// <summary>Game UI progress bar view (UIProgressBarComponent).</summary>
public readonly unsafe struct UIProgress
{
    private readonly Entity _entity;
    internal UIProgress(Entity entity) => _entity = entity;

    /// <summary>Bar fill, clamped to 0..1.</summary>
    public float Value
    {
        set => NativeApi.SetUIProgress(_entity.Id, value);
    }
}
