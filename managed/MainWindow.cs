using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Platform;
using Avalonia.Threading;

namespace MicrostructureStudio;

public sealed class MainWindow : Window
{
    private const int SimulationWidth = 800;
    private const int SimulationHeight = 600;

    private readonly IntPtr _simulation;
    private readonly WriteableBitmap _bitmap;
    private readonly DispatcherTimer _timer;
    private readonly Image _viewport;
    private readonly TextBlock _status;

    private readonly TextBox _rateBox;
    private readonly TextBox _minVelocityBox;
    private readonly TextBox _maxVelocityBox;

    private bool _running;
    private DateTime _lastTickUtc = DateTime.UtcNow;

    public MainWindow()
    {
        Title = "Microstructure Studio | Kinetic Voronoi Growth";
        Width = 1120;
        Height = 820;
        MinWidth = 850;
        MinHeight = 650;
        Background = new SolidColorBrush(Color.Parse("#11151D"));

        _simulation = NativeMethods.Create(SimulationWidth, SimulationHeight, 12345);
        if (_simulation == IntPtr.Zero)
            throw new InvalidOperationException("Could not create the native simulation.");

        _bitmap = new WriteableBitmap(
            new PixelSize(SimulationWidth, SimulationHeight),
            new Vector(96, 96),
            PixelFormat.Rgba8888,
            AlphaFormat.Unpremul);

        _viewport = new Image
        {
            Source = _bitmap,
            Stretch = Stretch.Uniform,
            HorizontalAlignment = HorizontalAlignment.Stretch,
            VerticalAlignment = VerticalAlignment.Stretch
        };

        _status = new TextBlock
        {
            Text = "Ready",
            Foreground = Brushes.Gainsboro,
            Margin = new Thickness(12, 8)
        };

        _rateBox = MakeInput("0.000012", 105);
        _minVelocityBox = MakeInput("35", 75);
        _maxVelocityBox = MakeInput("65", 75);

        var applyButton = MakeButton("Apply parameters");
        applyButton.Click += (_, _) => ApplyParameters();

        var startButton = MakeButton("Start");
        startButton.Click += (_, _) => ToggleRunning(startButton);

        var resetButton = MakeButton("Reset");
        resetButton.Click += (_, _) => ResetSimulation();

        var toolbar = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 9,
            Margin = new Thickness(12),
            VerticalAlignment = VerticalAlignment.Center
        };

        toolbar.Children.Add(MakeLabel("Nucleation rate"));
        toolbar.Children.Add(_rateBox);
        toolbar.Children.Add(MakeLabel("V min"));
        toolbar.Children.Add(_minVelocityBox);
        toolbar.Children.Add(MakeLabel("V max"));
        toolbar.Children.Add(_maxVelocityBox);
        toolbar.Children.Add(applyButton);
        toolbar.Children.Add(startButton);
        toolbar.Children.Add(resetButton);

        var viewportBorder = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#0B0E13")),
            BorderBrush = new SolidColorBrush(Color.Parse("#303846")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(8),
            Padding = new Thickness(8),
            Margin = new Thickness(12, 0, 12, 0),
            Child = _viewport
        };

        var root = new DockPanel();
        DockPanel.SetDock(toolbar, Dock.Top);
        DockPanel.SetDock(_status, Dock.Bottom);
        root.Children.Add(toolbar);
        root.Children.Add(_status);
        root.Children.Add(viewportBorder);
        Content = root;

        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(33) };
        _timer.Tick += OnTimerTick;
        _timer.Start();

        ApplyParameters();
        RenderFrame();
    }

    private static TextBox MakeInput(string value, double width)
        => new()
        {
            Text = value,
            Width = width,
            VerticalContentAlignment = VerticalAlignment.Center
        };

    private static TextBlock MakeLabel(string text)
        => new()
        {
            Text = text,
            Foreground = Brushes.Gainsboro,
            VerticalAlignment = VerticalAlignment.Center
        };

    private static Button MakeButton(string text)
        => new()
        {
            Content = text,
            Padding = new Thickness(12, 7),
            VerticalAlignment = VerticalAlignment.Center
        };

    private void ApplyParameters()
    {
        if (!TryRead(_rateBox.Text, out double rate) ||
            !TryRead(_minVelocityBox.Text, out double minV) ||
            !TryRead(_maxVelocityBox.Text, out double maxV) ||
            rate < 0 || minV <= 0 || maxV < minV)
        {
            _status.Text = "Invalid parameters. Require rate >= 0 and 0 < Vmin <= Vmax.";
            return;
        }

        NativeMethods.SetParameters(_simulation, rate, minV, maxV);
        _status.Text = $"Parameters applied | Rate: {rate:G4} nuclei/pixel/s | Velocity: {minV:G4}–{maxV:G4} px/s";
    }

    private static bool TryRead(string? text, out double value)
        => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value)
           && double.IsFinite(value);

    private void ToggleRunning(Button button)
    {
        _running = !_running;
        button.Content = _running ? "Pause" : "Start";
        _lastTickUtc = DateTime.UtcNow;
    }

    private void ResetSimulation()
    {
        NativeMethods.Reset(_simulation, (uint)Random.Shared.Next(1, int.MaxValue));
        _running = false;
        RenderFrame();
        _status.Text = "Simulation reset.";
    }

    private void OnTimerTick(object? sender, EventArgs e)
    {
        if (!_running) return;

        var now = DateTime.UtcNow;
        double dt = Math.Clamp((now - _lastTickUtc).TotalSeconds, 0.001, 0.1);
        _lastTickUtc = now;

        NativeMethods.Step(_simulation, dt);
        RenderFrame();

        double time = NativeMethods.GetSimulationTime(_simulation);
        int nuclei = NativeMethods.GetNucleusCount(_simulation);
        double coverage = NativeMethods.GetCoverage(_simulation) * 100.0;

        _status.Text = $"RUNNING  |  t = {time:F2} s  |  Nuclei = {nuclei}  |  Coverage = {coverage:F1}%";
    }

    private void RenderFrame()
    {
        using var framebuffer = _bitmap.Lock();
        NativeMethods.RenderRgba(_simulation, framebuffer.Address, framebuffer.RowBytes);
        _viewport.InvalidateVisual();
    }

    protected override void OnClosed(EventArgs e)
    {
        _timer.Stop();
        NativeMethods.Destroy(_simulation);
        _bitmap.Dispose();
        base.OnClosed(e);
    }
}
