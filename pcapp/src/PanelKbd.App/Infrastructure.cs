using System.Globalization;
using System.Windows;
using System.Windows.Data;
using System.Windows.Input;
using System.Windows.Media;
using PanelKbd.Core;

namespace PanelKbd.App;

public sealed class RelayCommand : ICommand
{
    readonly Action<object?> _run; readonly Func<object?, bool>? _can;
    public RelayCommand(Action<object?> run, Func<object?, bool>? can = null) { _run = run; _can = can; }
    public RelayCommand(Action run, Func<bool>? can = null) : this(_ => run(), can == null ? null : _ => can()) { }
    public event EventHandler? CanExecuteChanged { add => CommandManager.RequerySuggested += value; remove => CommandManager.RequerySuggested -= value; }
    public bool CanExecute(object? p) => _can?.Invoke(p) ?? true;
    public void Execute(object? p) => _run(p);
}

public sealed record EnumItem(object Value, string Text);

public sealed class BoolToVisibility : IValueConverter
{
    public object Convert(object v, Type t, object p, CultureInfo c)
    {
        bool b = v is true;
        if (p as string == "!") b = !b;
        return b ? Visibility.Visible : Visibility.Collapsed;
    }
    public object ConvertBack(object v, Type t, object p, CultureInfo c) => throw new NotSupportedException();
}

public sealed class NotConverter : IValueConverter
{
    public object Convert(object v, Type t, object p, CultureInfo c) => v is not true;
    public object ConvertBack(object v, Type t, object p, CultureInfo c) => v is not true;
}

/// <summary>Severity → màu chữ.</summary>
public sealed class SeverityBrush : IValueConverter
{
    public object Convert(object v, Type t, object p, CultureInfo c) => v is Severity s ? s switch
    {
        Severity.Error => Brushes.Firebrick,
        Severity.Warning => Brushes.DarkOrange,
        _ => Brushes.SteelBlue,
    } : Brushes.Black;
    public object ConvertBack(object v, Type t, object p, CultureInfo c) => throw new NotSupportedException();
}

public sealed class ErrorBrush : IValueConverter
{
    public object Convert(object v, Type t, object p, CultureInfo c) => v is true ? Brushes.Firebrick : Brushes.SeaGreen;
    public object ConvertBack(object v, Type t, object p, CultureInfo c) => throw new NotSupportedException();
}

/// <summary>Màn hình 7 đoạn vẽ bằng đồ họa vector.</summary>
public sealed class SevenSegmentDisplay : FrameworkElement
{
    public static readonly DependencyProperty TextProperty = DependencyProperty.Register(
        nameof(Text), typeof(string), typeof(SevenSegmentDisplay),
        new FrameworkPropertyMetadata("", FrameworkPropertyMetadataOptions.AffectsRender));
    public static readonly DependencyProperty DigitsProperty = DependencyProperty.Register(
        nameof(Digits), typeof(int), typeof(SevenSegmentDisplay),
        new FrameworkPropertyMetadata(4, FrameworkPropertyMetadataOptions.AffectsMeasure | FrameworkPropertyMetadataOptions.AffectsRender));
    public static readonly DependencyProperty DigitWidthProperty = DependencyProperty.Register(
        nameof(DigitWidth), typeof(double), typeof(SevenSegmentDisplay),
        new FrameworkPropertyMetadata(44.0, FrameworkPropertyMetadataOptions.AffectsMeasure | FrameworkPropertyMetadataOptions.AffectsRender));
    public static readonly DependencyProperty LitBrushProperty = DependencyProperty.Register(
        nameof(LitBrush), typeof(Brush), typeof(SevenSegmentDisplay),
        new FrameworkPropertyMetadata(new SolidColorBrush(Color.FromRgb(0xFF, 0x3B, 0x1E)), FrameworkPropertyMetadataOptions.AffectsRender));

    public static readonly DependencyProperty MasksProperty = DependencyProperty.Register(
        nameof(Masks), typeof(byte[]), typeof(SevenSegmentDisplay),
        new FrameworkPropertyMetadata(null, FrameworkPropertyMetadataOptions.AffectsRender));

    /// <summary>Nếu đặt: vẽ trực tiếp các mã 7 đoạn này (mô phỏng), bỏ qua Text.</summary>
    public byte[]? Masks { get => (byte[]?)GetValue(MasksProperty); set => SetValue(MasksProperty, value); }

    public string Text { get => (string)GetValue(TextProperty); set => SetValue(TextProperty, value); }
    public int Digits { get => (int)GetValue(DigitsProperty); set => SetValue(DigitsProperty, value); }
    public double DigitWidth { get => (double)GetValue(DigitWidthProperty); set => SetValue(DigitWidthProperty, value); }
    public Brush LitBrush { get => (Brush)GetValue(LitBrushProperty); set => SetValue(LitBrushProperty, value); }

    static readonly Brush Unlit = new SolidColorBrush(Color.FromRgb(0x2E, 0x14, 0x10));

    protected override Size MeasureOverride(Size availableSize) => new(Digits * DigitWidth, DigitWidth * 1.7);

    protected override void OnRender(DrawingContext dc)
    {
        double w = DigitWidth, h = DigitWidth * 1.7;
        var masks = Masks ?? SevenSeg.Layout(Text ?? "", Math.Max(1, Digits));
        for (int i = 0; i < Math.Min(masks.Length, Digits); i++) DrawDigit(dc, i * w, 0, w, h, masks[i]);
    }

    void DrawDigit(DrawingContext dc, double x, double y, double w, double h, byte mask)
    {
        double t = w * 0.15, g = t * 0.35;
        double xl = x + t * 0.9, xr = x + w - t * 2.6, yt = y + t * 1.1, ym = y + h / 2, yb = y + h - t * 1.1;
        void Hor(double x0, double x1, double yc, bool on) =>
            dc.DrawGeometry(on ? LitBrush : Unlit, null, Poly(
                (x0, yc), (x0 + t / 2, yc - t / 2), (x1 - t / 2, yc - t / 2), (x1, yc), (x1 - t / 2, yc + t / 2), (x0 + t / 2, yc + t / 2)));
        void Ver(double y0, double y1, double xc, bool on) =>
            dc.DrawGeometry(on ? LitBrush : Unlit, null, Poly(
                (xc, y0), (xc + t / 2, y0 + t / 2), (xc + t / 2, y1 - t / 2), (xc, y1), (xc - t / 2, y1 - t / 2), (xc - t / 2, y0 + t / 2)));
        bool S(int bit) => (mask & (1 << bit)) != 0;
        Hor(xl + g, xr - g, yt, S(0));          // a
        Ver(yt + g, ym - g, xr, S(1));          // b
        Ver(ym + g, yb - g, xr, S(2));          // c
        Hor(xl + g, xr - g, yb, S(3));          // d
        Ver(ym + g, yb - g, xl, S(4));          // e
        Ver(yt + g, ym - g, xl, S(5));          // f
        Hor(xl + g, xr - g, ym, S(6));          // g
        dc.DrawEllipse((mask & SevenSeg.Dp) != 0 ? LitBrush : Unlit, null, new Point(xr + t * 1.5, yb), t * 0.5, t * 0.5);
    }

    static Geometry Poly(params (double X, double Y)[] pts)
    {
        var f = new PathFigure { StartPoint = new Point(pts[0].X, pts[0].Y), IsClosed = true };
        for (int i = 1; i < pts.Length; i++) f.Segments.Add(new LineSegment(new Point(pts[i].X, pts[i].Y), false));
        var g = new PathGeometry(); g.Figures.Add(f); g.Freeze();
        return g;
    }
}
