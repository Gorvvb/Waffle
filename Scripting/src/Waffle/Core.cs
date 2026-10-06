using System.Numerics;

namespace Waffle;

/// <summary>Keyboard codes (GLFW values, identical to the engine's Key table).</summary>
public enum KeyCode : ushort
{
    None = 0,
    Space = 32, Apostrophe = 39, Comma = 44, Minus = 45, Period = 46, Slash = 47,
    D0 = 48, D1 = 49, D2 = 50, D3 = 51, D4 = 52, D5 = 53, D6 = 54, D7 = 55, D8 = 56, D9 = 57,
    Semicolon = 59, Equal = 61,
    A = 65, B = 66, C = 67, D = 68, E = 69, F = 70, G = 71, H = 72, I = 73, J = 74,
    K = 75, L = 76, M = 77, N = 78, O = 79, P = 80, Q = 81, R = 82, S = 83, T = 84,
    U = 85, V = 86, W = 87, X = 88, Y = 89, Z = 90,
    LeftBracket = 91, Backslash = 92, RightBracket = 93, GraveAccent = 96,
    Escape = 256, Enter = 257, Tab = 258, Backspace = 259, Insert = 260, Delete = 261,
    Right = 262, Left = 263, Down = 264, Up = 265,
    PageUp = 266, PageDown = 267, Home = 268, End = 269,
    CapsLock = 280, ScrollLock = 281, NumLock = 282, PrintScreen = 283, Pause = 284,
    F1 = 290, F2 = 291, F3 = 292, F4 = 293, F5 = 294, F6 = 295,
    F7 = 296, F8 = 297, F9 = 298, F10 = 299, F11 = 300, F12 = 301,
    LeftShift = 340, LeftControl = 341, LeftAlt = 342, LeftSuper = 343,
    RightShift = 344, RightControl = 345, RightAlt = 346, RightSuper = 347,
    Menu = 348,
}

/// <summary>Mouse button codes.</summary>
public enum MouseButton : int
{
    Button0 = 0, Button1 = 1, Button2 = 2, Button3 = 3,
    Button4 = 4, Button5 = 5, Button6 = 6, Button7 = 7,
    Left = 0, Right = 1, Middle = 2,
}

/// <summary>Rigidbody motion type (mirrors Box2D).</summary>
public enum BodyType : int
{
    Static = 0,
    Kinematic = 1,
    Dynamic = 2,
}

/// <summary>RGBA color, 0-1 range.</summary>
public readonly struct Color
{
    public readonly float R, G, B, A;
    public Color(float r, float g, float b, float a = 1f) { R = r; G = g; B = b; A = a; }

    public static Color White => new(1, 1, 1);
    public static Color Black => new(0, 0, 0);
    public static Color Red => new(1, 0, 0);
    public static Color Green => new(0, 1, 0);
    public static Color Blue => new(0, 0, 1);
    public static Color Yellow => new(1, 0.85f, 0.15f);
}

/// <summary>Math helpers for gameplay code.</summary>
public static class Mathf
{
    public const float Pi = MathF.PI;
    public const float Deg2Rad = MathF.PI / 180f;
    public const float Rad2Deg = 180f / MathF.PI;

    public static float Lerp(float a, float b, float t) => a + (b - a) * t;
    public static float Clamp(float v, float min, float max) => MathF.Min(max, MathF.Max(min, v));
    public static int Clamp(int v, int min, int max) => Math.Min(max, Math.Max(min, v));
    public static float Sign(float v) => MathF.Sign(v);
    public static float Abs(float v) => MathF.Abs(v);
    public static float Sqrt(float v) => MathF.Sqrt(v);
    public static float Floor(float v) => MathF.Floor(v);
    public static float Ceil(float v) => MathF.Ceiling(v);
    public static float Round(float v) => MathF.Round(v);
    public static float Sin(float v) => MathF.Sin(v);
    public static float Cos(float v) => MathF.Cos(v);
    public static float Atan2(float y, float x) => MathF.Atan2(y, x);
    public static float Min(float a, float b) => MathF.Min(a, b);
    public static float Max(float a, float b) => MathF.Max(a, b);
    public static float Random(float min, float max) => min + (max - min) * System.Random.Shared.NextSingle();
    public static int RandomInt(int minInclusive, int maxExclusive) => System.Random.Shared.Next(minInclusive, maxExclusive);
}

/// <summary>Vector2 helpers beyond System.Numerics.</summary>
public static class Vec2
{
    public static float Length(Vector2 v) => v.Length();
    public static float LengthSq(Vector2 v) => v.LengthSquared();
    public static Vector2 Normalize(Vector2 v) => v.Length() > 0 ? v / v.Length() : Vector2.Zero;
    public static float Dot(Vector2 a, Vector2 b) => Vector2.Dot(a, b);
    public static float Distance(Vector2 a, Vector2 b) => Vector2.Distance(a, b);
    public static float DistanceSq(Vector2 a, Vector2 b) => Vector2.DistanceSquared(a, b);
    public static Vector2 Lerp(Vector2 a, Vector2 b, float t) => a + (b - a) * t;
    public static float Angle(Vector2 v) => MathF.Atan2(v.Y, v.X);
    public static Vector2 FromAngle(float radians) => new(MathF.Cos(radians), MathF.Sin(radians));
}

/// <summary>Inspector attributes for script fields.</summary>
[AttributeUsage(AttributeTargets.Field)]
public sealed class RangeAttribute : Attribute
{
    public float Min { get; }
    public float Max { get; }
    public RangeAttribute(float min, float max) { Min = min; Max = max; }
}

[AttributeUsage(AttributeTargets.Field)]
public sealed class TooltipAttribute : Attribute
{
    public string Text { get; }
    public TooltipAttribute(string text) { Text = text; }
}
