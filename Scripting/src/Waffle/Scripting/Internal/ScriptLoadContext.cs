using System.Reflection;
using System.Runtime.Loader;

namespace Waffle.Scripting.Internal;

/// <summary>
/// Collectible load context that owns ONLY the game-scripts assembly.
/// Everything else - the contract assembly (Waffle.Scripting), the framework -
/// must fall through to the default context: the engine host already loaded
/// Waffle.Scripting via hostfxr, and type identity of WaffleBehaviour has to
/// match it or script classes would silently not derive from it.
/// </summary>
internal sealed class ScriptLoadContext : AssemblyLoadContext
{
    // Optional: only exists when a deps.json sits next to the scripts
    // assembly (exported games). csc-built dev assemblies have none.
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
        // The host loaded the contract assembly by path through hostfxr, which
        // does NOT register it for by-name binding - so hand the binder the
        // exact already-loaded instance. Framework assemblies: default context.
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
