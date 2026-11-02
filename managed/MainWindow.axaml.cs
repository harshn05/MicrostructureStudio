using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;

namespace MicrostructureStudio;

public partial class MainWindow : Window
{
    private const int DefaultWidth = 800;
    private const int DefaultHeight = 600;
    private const int MaxDimension = 4096;
    private const double CompletionCoverage = 1.0; // 100.00%

    private IntPtr _simulation;
    private WriteableBitmap? _bitmap;
    private DispatcherTimer _timer;
    private int _simulationWidth = DefaultWidth;
    private int _simulationHeight = DefaultHeight;
    private bool _running;
    private DateTime _lastTickUtc = DateTime.UtcNow;

    public MainWindow()
    {
        InitializeComponent();

        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(33) };
        _timer.Tick += OnTimerTick;

        if (Design.IsDesignMode)
        {
            StatusText.Text = "DESIGN PREVIEW | Native engine disabled";
            return;
        }

        if (!CreateSimulation(_simulationWidth, _simulationHeight))
            throw new InvalidOperationException("Could not create the native simulation.");

        ApplyButton.Click += (_, _) => ApplyParameters();
        ApplyDimensionsButton.Click += (_, _) => ApplyDimensions();
        StartButton.Click += (_, _) => ToggleRunning();
        ResetButton.Click += (_, _) => ResetSimulation();

        ApplyParameters();
        RenderFrame();

        // Start the timer after registering its Tick handler.
        // It remains idle while paused because OnTimerTick checks _running.
        _timer.Start();
    }

    private bool CreateSimulation(int width, int height)
    {
        IntPtr newSimulation = NativeMethods.Create(
            width, height, (uint)Random.Shared.Next(1, int.MaxValue));

        if (newSimulation == IntPtr.Zero)
            return false;

        var newBitmap = new WriteableBitmap(
            new PixelSize(width, height),
            new Vector(96, 96),
            PixelFormat.Rgba8888,
            AlphaFormat.Unpremul);

        if (_simulation != IntPtr.Zero)
            NativeMethods.Destroy(_simulation);
        _bitmap?.Dispose();

        _simulation = newSimulation;
        _bitmap = newBitmap;
        _simulationWidth = width;
        _simulationHeight = height;
        Viewport.Source = _bitmap;
        Viewport.InvalidateVisual();
        UpdateDimensionLabel();
        return true;
    }

    private void ApplyDimensions()
    {
        if (!int.TryParse(XPixelsBox.Text, NumberStyles.Integer,
                CultureInfo.InvariantCulture, out int width) ||
            !int.TryParse(YPixelsBox.Text, NumberStyles.Integer,
                CultureInfo.InvariantCulture, out int height) ||
            width < 64 || height < 64 ||
            width > MaxDimension || height > MaxDimension)
        {
            StatusText.Text = $"Invalid dimensions. Enter X and Y between 64 and {MaxDimension}.";
            return;
        }

        _running = false;
        StartButton.Content = "▶ Start";

        if (!CreateSimulation(width, height))
        {
            StatusText.Text = "Could not allocate simulation at the requested resolution.";
            return;
        }

        ApplyParameters();
        RenderFrame();
        StatusText.Text = $"New simulation created: {width} × {height} pixels.";
    }

    private void UpdateDimensionLabel()
    {
        DimensionLabel.Text = $"{_simulationWidth} × {_simulationHeight} px";
    }

    private void ApplyParameters()
    {
        if (_simulation == IntPtr.Zero)
            return;

        if (!double.TryParse(RateBox.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double rate) ||
            !double.TryParse(MinVelocityBox.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double minV) ||
            !double.TryParse(MaxVelocityBox.Text, NumberStyles.Float, CultureInfo.InvariantCulture, out double maxV) ||
            !double.IsFinite(rate) || !double.IsFinite(minV) || !double.IsFinite(maxV) ||
            rate < 0 || minV <= 0 || maxV < minV)
        {
            StatusText.Text = "Invalid parameters: rate >= 0 and 0 < Vmin <= Vmax.";
            return;
        }

        NativeMethods.SetParameters(_simulation, rate, minV, maxV);
        StatusText.Text = $"Parameters applied | Rate: {rate:G4} | Velocity: {minV:G4}–{maxV:G4} px/s";
    }

    private void ToggleRunning()
    {
        if (_simulation == IntPtr.Zero)
            return;

        if (NativeMethods.GetCoverage(_simulation) >= CompletionCoverage)
        {
            StatusText.Text = "Simulation complete. Reset to start a new run.";
            return;
        }

        _running = !_running;
        StartButton.Content = _running ? "Ⅱ Pause" : "▶ Start";
        _lastTickUtc = DateTime.UtcNow;
        StatusText.Text = _running ? "RUNNING | Initializing simulation..." : "PAUSED";
    }

    private void ResetSimulation()
    {
        if (_simulation == IntPtr.Zero)
            return;

        NativeMethods.Reset(_simulation, (uint)Random.Shared.Next(1, int.MaxValue));
        _running = false;
        StartButton.Content = "▶ Start";
        RenderFrame();
        StatusText.Text = $"Reset | Domain: {_simulationWidth} × {_simulationHeight} px";
    }

    private void OnTimerTick(object? sender, EventArgs e)
    {
        if (!_running || _simulation == IntPtr.Zero)
            return;

        // Stop before another native step once the domain is effectively transformed.
        double coverageBefore = NativeMethods.GetCoverage(_simulation);
        if (coverageBefore >= CompletionCoverage)
        {
            CompleteSimulation(coverageBefore);
            return;
        }

        DateTime now = DateTime.UtcNow;
        double dt = Math.Clamp((now - _lastTickUtc).TotalSeconds, 0.001, 0.1);
        _lastTickUtc = now;

        NativeMethods.Step(_simulation, dt);
        RenderFrame();

        double time = NativeMethods.GetSimulationTime(_simulation);
        int nuclei = NativeMethods.GetNucleusCount(_simulation);
        double coverage = NativeMethods.GetCoverage(_simulation);

        if (coverage >= CompletionCoverage)
        {
            CompleteSimulation(coverage);
            return;
        }

        StatusText.Text =
            $"RUNNING | t = {time:F2} s | Nuclei = {nuclei} | Coverage = {coverage * 100:F2}%";
    }

    private void CompleteSimulation(double coverage)
    {
        _running = false;
        StartButton.Content = "▶ Start";
        StatusText.Text =
            $"COMPLETE | t = {NativeMethods.GetSimulationTime(_simulation):F2} s | " +
            $"Nuclei = {NativeMethods.GetNucleusCount(_simulation)} | " +
            $"Coverage = {coverage * 100:F2}% | Reset to run again";
    }

    private void RenderFrame()
    {
        if (_bitmap is null || _simulation == IntPtr.Zero)
            return;

        using var framebuffer = _bitmap.Lock();
        NativeMethods.RenderRgba(_simulation, framebuffer.Address, framebuffer.RowBytes);
        Viewport.InvalidateVisual();
    }

    protected override void OnClosed(EventArgs e)
    {
        _timer.Stop();
        if (_simulation != IntPtr.Zero)
            NativeMethods.Destroy(_simulation);
        _bitmap?.Dispose();
        base.OnClosed(e);
    }
}
