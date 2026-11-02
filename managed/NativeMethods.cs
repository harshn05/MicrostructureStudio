using System.Runtime.InteropServices;

namespace MicrostructureStudio;

internal static class NativeMethods
{
    private const string Dll = "MicrostructureCore.dll";

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_create")]
    internal static extern IntPtr Create(int w, int h, uint seed);

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_destroy")]
    internal static extern void Destroy(IntPtr p);

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_reset")]
    internal static extern void Reset(IntPtr p, uint seed);

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_set_parameters")]
    internal static extern void SetParameters(
        IntPtr p,
        double rate,
        double min,
        double max);

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_step")]
    internal static extern void Step(IntPtr p, double dt);

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_render_rgba")]
    internal static extern void RenderRgba(
        IntPtr p,
        IntPtr dst,
        int stride);

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_get_nucleus_count")]
    internal static extern int GetNucleusCount(IntPtr p);

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_get_simulation_time")]
    internal static extern double GetSimulationTime(IntPtr p);

    [DllImport(
        Dll,
        CallingConvention = CallingConvention.Cdecl,
        EntryPoint = "ms_get_coverage")]
    internal static extern double GetCoverage(IntPtr p);
}