using System.Runtime.InteropServices;

namespace MicrostructureStudio;

internal static class NativeMethods
{
    private const string Library = "MicrostructureCore.dll";

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_create")]
    internal static extern IntPtr Create(int width, int height, uint seed);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_destroy")]
    internal static extern void Destroy(IntPtr simulation);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_reset")]
    internal static extern void Reset(IntPtr simulation, uint seed);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_set_parameters")]
    internal static extern void SetParameters(IntPtr simulation, double nucleationRate,
        double minGrowthVelocity, double maxGrowthVelocity);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_step")]
    internal static extern void Step(IntPtr simulation, double deltaTime);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_render_rgba")]
    internal static extern void RenderRgba(IntPtr simulation, IntPtr destination, int stride);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_get_nucleus_count")]
    internal static extern int GetNucleusCount(IntPtr simulation);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_get_simulation_time")]
    internal static extern double GetSimulationTime(IntPtr simulation);

    [DllImport(Library, CallingConvention = CallingConvention.Cdecl, EntryPoint = "ms_get_coverage")]
    internal static extern double GetCoverage(IntPtr simulation);
}
