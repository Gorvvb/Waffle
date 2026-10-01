namespace Waffle;

/// <summary>Script-facing logging. Routes into the engine's spdlog console.</summary>
public static class Log
{
    public static void Info(string message) => Scripting.Internal.ScriptRuntime.LogInfo(message);
    public static void Warn(string message) => Scripting.Internal.ScriptRuntime.LogWarn(message);
    public static void Error(string message) => Scripting.Internal.ScriptRuntime.LogError(message);
}
