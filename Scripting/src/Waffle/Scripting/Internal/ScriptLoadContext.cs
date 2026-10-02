using System.Reflection;
using System.Runtime.Loader;

namespace Waffle.Scripting.Internal;

/// <summary>Collectible ALC owning only the game-scripts assembly; Waffle.Scripting and the framework must fall through to the default context or WaffleBehaviour type identity breaks.</summary>
internal sealed class ScriptLoadContext : AssemblyLoadContext
{
    // Only exists when a deps.json sits next to the scripts assembly (exported games); csc dev builds have none.
    private readonly AssemblyDependencyResolver? _resolver;

    public ScriptLoadContext(string mainAssemblyPath)
        : base("WaffleScripts", isCollectible: true)
    {
        try
        {
            _resolver = new AssemblyDependencyResolver(mainAssemblyPath);
        }
        catch
        {
            _resolver = null;
        }
    }

    protected override Assembly? Load(AssemblyName name)
    {
        // The host loaded Waffle.Scripting by path via hostfxr, so it is not registered for by-name binding - hand back the loaded instance. Framework assemblies: default context.
        if (name.Name == "Waffle.Scripting")
            return typeof(WaffleBehaviour).Assembly;

        string? simple = name.Name;
        if (simple is null ||
            simple == "netstandard" ||
            simple.StartsWith("System") ||
            simple.StartsWith("Microsoft"))
        {
            return null;
        }

        // Third-party managed dependencies of the script project, if any.
        string? path = _resolver?.ResolveAssemblyToPath(name);
        return path is not null ? LoadFromAssemblyPath(path) : null;
    }

    protected override IntPtr LoadUnmanagedDll(string name)
    {
        string? path = _resolver?.ResolveUnmanagedDllToPath(name);
        return path is not null ? LoadUnmanagedDllFromPath(path) : IntPtr.Zero;
    }
}
