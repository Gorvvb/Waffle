using System.Numerics;

namespace Waffle;

/// <summary>Classic 2D steering behaviors for enemies and NPCs - every method returns a desired velocity to assign to Rigidbody2D.Velocity (or blend before applying).</summary>
public static class Steering2D
{
    /// <summary>Full speed straight at the target.</summary>
    public static Vector2 Seek(Vector2 position, Vector2 target, float maxSpeed)
    {
        Vector2 desired = target - position;
        return desired.LengthSquared() > 1e-8f ? Vector2.Normalize(desired) * maxSpeed : Vector2.Zero;
    }

    /// <summary>Full speed straight away from the target.</summary>
    public static Vector2 Flee(Vector2 position, Vector2 target, float maxSpeed)
        => Seek(position, position * 2f - target, maxSpeed);

    /// <summary>Seek that decelerates to a stop inside the slow radius.</summary>
    public static Vector2 Arrive(Vector2 position, Vector2 target, float maxSpeed, float slowRadius)
    {
        Vector2 toTarget = target - position;
        float distance = toTarget.Length();
        if (distance < 1e-4f || slowRadius <= 0f)
            return Vector2.Zero;
        float speed = maxSpeed * MathF.Min(1f, distance / slowRadius);
        return toTarget / distance * speed;
    }

    /// <summary>Seek towards where the target WILL be (lead the shot); prediction caps at maxPrediction seconds.</summary>
    public static Vector2 Pursue(Vector2 position, Vector2 target, Vector2 targetVelocity, float maxSpeed, float maxPrediction = 0.5f)
    {
        Vector2 toTarget = target - position;
        float distance = toTarget.Length();
        float speed = MathF.Max(1e-4f, targetVelocity.Length());
        float prediction = MathF.Min(maxPrediction, distance / speed);
        return Seek(position, target + targetVelocity * prediction, maxSpeed);
    }

    /// <summary>Flee from where the target will be - the defensive twin of Pursue.</summary>
    public static Vector2 Evade(Vector2 position, Vector2 target, Vector2 targetVelocity, float maxSpeed, float maxPrediction = 0.5f)
    {
        Vector2 toTarget = target - position;
        float distance = toTarget.Length();
        float speed = MathF.Max(1e-4f, targetVelocity.Length());
        float prediction = MathF.Min(maxPrediction, distance / speed);
        return Flee(position, target + targetVelocity * prediction, maxSpeed);
    }

    /// <summary>Random smooth wandering: keeps a heading that drifts within a circle ahead of the agent. Pass the same wanderAngle by ref every frame (per-agent storage, e.g. on the script or its Blackboard).</summary>
    public static Vector2 Wander(Vector2 velocity, ref float wanderAngle, float maxSpeed,
        float jitter = 0.4f, float circleDistance = 2f, float circleRadius = 1.5f)
    {
        // wanderAngle is a world-space heading in radians; jitter it and steer towards a point on the circle ahead.
        wanderAngle += (RandomValue() * 2f - 1f) * jitter;
        Vector2 heading = velocity.LengthSquared() > 1e-8f
            ? Vector2.Normalize(velocity)
            : new Vector2(MathF.Cos(wanderAngle), MathF.Sin(wanderAngle));
        Vector2 circleCenter = heading * circleDistance;
        Vector2 offset = new Vector2(MathF.Cos(wanderAngle), MathF.Sin(wanderAngle)) * circleRadius;
        Vector2 desired = circleCenter + offset;
        return desired.LengthSquared() > 1e-8f ? Vector2.Normalize(desired) * maxSpeed : Vector2.Zero;
    }

    /// <summary>Pushes away from neighbors crowding within separationRadius (pass e.g. Physics2D.OverlapCircle positions); returns Zero when alone.</summary>
    public static Vector2 Separate(Vector2 position, System.Collections.Generic.IReadOnlyList<Vector2> neighborPositions,
        float separationRadius, float maxSpeed)
    {
        Vector2 push = Vector2.Zero;
        for (int i = 0; i < neighborPositions.Count; i++)
        {
            Vector2 away = position - neighborPositions[i];
            float distance = away.Length();
            if (distance > 1e-4f && distance < separationRadius)
                push += away / distance * (1f - distance / separationRadius);
        }
        return push.LengthSquared() > 1e-8f ? Vector2.Normalize(push) * maxSpeed : Vector2.Zero;
    }

    // Deterministic per-frame jitter source (wander must be reproducible for a fixed seed scenario, unlike Random).
    private static float RandomValue()
    {
        // xorshift32 on a static state - cheap, deterministic across runs.
        _wanderState ^= _wanderState << 13;
        _wanderState ^= _wanderState >> 17;
        _wanderState ^= _wanderState << 5;
        return (_wanderState & 0xFFFFFF) / (float)0x1000000;
    }
    private static uint _wanderState = 0x9E3779B9;
}
