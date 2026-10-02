using System.Diagnostics;

namespace Waffle.Scripting.Internal;

/// <summary>Compiles Assets/Scripts/**/*.cs to GameScripts.dll via the .NET SDK's Roslyn csc.dll (no csproj/MSBuild); skips when the output is newer than every source.</summary>
internal static class CompilerService
{
    /// <summary>Path of the most recently requested output assembly.</summary>
    internal static string LastOutputPath { get; private set; } = string.Empty;

    /// <summary>Points the loader at a pre-compiled scripts assembly in packed exports (nothing to compile).</summary>
    internal static void SetLastOutputPath(string path) => LastOutputPath = path;

    private static bool _sdkMissingLogged;

    /// <summary>Return codes (mirrored by the C++ host): 0 = compiled, 2 = up to date, -2 = no SDK, 1 = failed (see ScriptRuntime.GetLastError).</summary>
    public static int Compile(string sourcesDir, string outputDll)
    {
        // The runtime's assembly loader requires absolute paths.
        sourcesDir = Path.GetFullPath(sourcesDir);
        outputDll = Path.GetFullPath(outputDll);
        LastOutputPath = outputDll;

        if (!Directory.Exists(sourcesDir))
        {
            ScriptRuntime.SetLastError($"scripts source folder '{sourcesDir}' does not exist");
            return 1;
        }

        List<string> sources = Directory.EnumerateFiles(sourcesDir, "*.cs", SearchOption.AllDirectories)
            .OrderBy(p => p, StringComparer.OrdinalIgnoreCase)
            .ToList();
        if (sources.Count == 0)
            return 2; // project has no scripts - nothing to do

        string? dllDir = Path.GetDirectoryName(Path.GetFullPath(outputDll));
        if (dllDir is not null)
            Directory.CreateDirectory(dllDir);

        // Freshness: skip when the output is newer than every source.
        if (File.Exists(outputDll))
        {
            DateTime outputTime = File.GetLastWriteTimeUtc(outputDll);
            bool stale = sources.Any(src => File.GetLastWriteTimeUtc(src) > outputTime);
            if (!stale)
                return 2;
        }

        (string dotnet, string sdkBase, string sdkVersion)? sdk = FindSdk();
        if (sdk is null)
        {
            if (!_sdkMissingLogged)
            {
                ScriptRuntime.LogWarn("CompilerService: no .NET SDK found - cannot compile scripts (using last compiled assembly)");
                _sdkMissingLogged = true;
            }
            return -2;
        }

        string csc = Path.Combine(sdk.Value.sdkBase, sdk.Value.sdkVersion, "Roslyn", "bincore", "csc.dll");
        if (!File.Exists(csc))
        {
            ScriptRuntime.SetLastError($"Roslyn compiler not found at {csc}");
            return -2;
        }

        List<string> references = CollectReferences();
        references.Add(typeof(WaffleBehaviour).Assembly.Location);

        string pdb = Path.ChangeExtension(outputDll, ".pdb");
        string rspPath = Path.Combine(dllDir!, "csc.rsp");

        var args = new List<string>
        {
            "-nologo",
            "-nostdlib+",
            "-target:library",
            $"-out:\"{outputDll}\"",
            $"-pdb:\"{pdb}\"",
            "-debug:portable",
            "-unsafe+",
            "-nullable+",
            "-langversion:latest",
            "-define:TRACE",
        };
        args.AddRange(references.Select(r => $"-r:\"{r}\""));
        args.AddRange(sources.Select(s => $"\"{s}\""));
        File.WriteAllLines(rspPath, args);

        var psi = new ProcessStartInfo
        {
            FileName = sdk.Value.dotnet,
            Arguments = $"exec \"{csc}\" -noconfig \"@{rspPath}\"",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        using Process? process = Process.Start(psi);
        if (process is null)
        {
            ScriptRuntime.SetLastError("failed to start the compiler process");
            return 1;
        }
        string stdout = process.StandardOutput.ReadToEnd();
        string stderr = process.StandardError.ReadToEnd();
        if (!process.WaitForExit(30000))
        {
            process.Kill();
            ScriptRuntime.SetLastError("compiler timed out");
            return 1;
        }

        if (process.ExitCode != 0 || !File.Exists(outputDll))
        {
            string message = (stderr.Length > 0 ? stderr : stdout);
            // Keep it console-friendly: first ~40 lines.
            var lines = message.Split('\n', StringSplitOptions.RemoveEmptyEntries);
            string summary = string.Join("\n", lines.Take(40));
            ScriptRuntime.SetLastError(string.IsNullOrWhiteSpace(summary) ? "compiler failed" : summary);
            return 1;
        }

        ScriptRuntime.SetLastError(string.Empty);
        return 0;
    }

    private static (string Dotnet, string SdkBase, string SdkVersion)? FindSdk()
    {
        string? dotnet;
        string exeName = OperatingSystem.IsWindows() ? "dotnet.exe" : "dotnet";
        string pathVar = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        dotnet = pathVar
            .Split(OperatingSystem.IsWindows() ? ';' : ':', StringSplitOptions.RemoveEmptyEntries)
            .Select(dir => Path.Combine(dir.Trim(), exeName))
            .FirstOrDefault(File.Exists);
        if (dotnet is null)
            return null;

        var psi = new ProcessStartInfo
        {
            FileName = dotnet,
            Arguments = "--list-sdks",
            UseShellExecute = false,
            RedirectStandardOutput = true,
            CreateNoWindow = true,
        };
        using Process? process = Process.Start(psi);
        if (process is null || !process.WaitForExit(15000))
            return null;
        string output = process.StandardOutput.ReadToEnd();

        (string Base, string Version)? best = null;
        foreach (string rawLine in output.Split('\n', StringSplitOptions.RemoveEmptyEntries))
        {
            string line = rawLine.Trim();
            int bracket = line.IndexOf('[');
            if (bracket <= 0)
                continue;
            string version = line[..bracket].Trim();
            string location = line[(bracket + 1)..].Trim().TrimEnd(']');
            if (!Version.TryParse(version, out _))
                continue;
            if (best is null || string.CompareOrdinal(version, best.Value.Version) > 0)
                best = (location, version);
        }
        if (best is null)
            return null;
        return (dotnet, best.Value.Base, best.Value.Version);
    }

    private static List<string> CollectReferences()
    {
        // Trusted platform assemblies = the exact framework set this process loaded; all managed.
        var references = new List<string>();
        if (AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES") is string tpa)
        {
            char separator = Path.PathSeparator;
            references.AddRange(tpa.Split(separator, StringSplitOptions.RemoveEmptyEntries)
                .Where(p => p.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) && File.Exists(p)));
        }
        return references;
    }
}
