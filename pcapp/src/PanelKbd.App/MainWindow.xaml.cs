using System.ComponentModel;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using PanelKbd.Core;

namespace PanelKbd.App;

public partial class MainWindow : Window
{
    readonly MainViewModel _vm = new();

    public MainWindow()
    {
        DataContext = _vm;
        InitializeComponent();
    }

    protected override void OnClosing(CancelEventArgs e)
    {
        if (!_vm.ConfirmDiscard()) e.Cancel = true; else _vm.ShutdownSim();
        base.OnClosing(e);
    }

    void Issue_DoubleClick(object sender, MouseButtonEventArgs e)
    {
        if (sender is ListView lv && lv.SelectedItem is Issue i) _vm.GotoIssueCmd.Execute(i);
    }

    void SimKey_Down(object sender, MouseButtonEventArgs e)
    {
        if (sender is Button b && b.Tag is string id) _vm.SimPress(id, true);
    }

    void SimKey_Up(object sender, MouseEventArgs e)
    {
        if (sender is Button b && b.Tag is string id) _vm.SimPress(id, false);
    }

    void RefreshHw_Click(object sender, RoutedEventArgs e) => _vm.RefreshPortsCmd.Execute(null);

    static readonly Dictionary<Key, string> PcKeys = new()
    {
        [Key.D0] = "0", [Key.D1] = "1", [Key.D2] = "2", [Key.D3] = "3", [Key.D4] = "4", [Key.D5] = "5", [Key.D6] = "6", [Key.D7] = "7", [Key.D8] = "8", [Key.D9] = "9",
        [Key.NumPad0] = "0", [Key.NumPad1] = "1", [Key.NumPad2] = "2", [Key.NumPad3] = "3", [Key.NumPad4] = "4",
        [Key.NumPad5] = "5", [Key.NumPad6] = "6", [Key.NumPad7] = "7", [Key.NumPad8] = "8", [Key.NumPad9] = "9",
        [Key.Escape] = "ESC", [Key.Enter] = "ENT", [Key.Up] = "UP", [Key.Down] = "DN", [Key.F1] = "F1", [Key.F2] = "F2", [Key.F3] = "K3",
    };

    bool PcKeyTarget(KeyEventArgs e, out string id)
    {
        id = "";
        if (_vm.SelectedTab is not (MainViewModel.TabSim or MainViewModel.TabHw)) return false;
        if (Keyboard.FocusedElement is TextBox or ComboBox or DataGridCell) return false;
        var k = e.Key == Key.System ? e.SystemKey : e.Key;
        return PcKeys.TryGetValue(k, out id!);
    }

    protected override void OnPreviewKeyDown(KeyEventArgs e)
    {
        if (!e.IsRepeat && PcKeyTarget(e, out var id))
        {
            if (_vm.SelectedTab == MainViewModel.TabSim) _vm.SimPress(id, true); else _vm.HwVirtualPress(id, true);
            e.Handled = true;
        }
        base.OnPreviewKeyDown(e);
    }

    protected override void OnPreviewKeyUp(KeyEventArgs e)
    {
        if (PcKeyTarget(e, out var id))
        {
            if (_vm.SelectedTab == MainViewModel.TabSim) _vm.SimPress(id, false); else _vm.HwVirtualPress(id, false);
            e.Handled = true;
        }
        base.OnPreviewKeyUp(e);
    }

    void Log_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (sender is TextBox tb) tb.ScrollToEnd();
    }
}

/// <summary>Phím cố định trên hình bàn phím (không cấu hình được).</summary>
public sealed class KeyCap : Border
{
    public static readonly DependencyProperty LabelProperty = DependencyProperty.Register(
        nameof(Label), typeof(string), typeof(KeyCap), new PropertyMetadata("", (d, e) => ((KeyCap)d).Update()));

    public string Label { get => (string)GetValue(LabelProperty); set => SetValue(LabelProperty, value); }

    public KeyCap()
    {
        Width = 62; Height = 48; Margin = new Thickness(4);
        CornerRadius = new CornerRadius(6);
        Background = new SolidColorBrush(Color.FromRgb(0x2A, 0x31, 0x3D));
        BorderBrush = new SolidColorBrush(Color.FromRgb(0x3A, 0x43, 0x52));
        BorderThickness = new Thickness(2);
        Update();
    }

    void Update() => Child = new TextBlock
    {
        Text = Label, Foreground = new SolidColorBrush(Color.FromRgb(0xC8, 0xD0, 0xDC)), FontSize = 16,
        HorizontalAlignment = HorizontalAlignment.Center, VerticalAlignment = VerticalAlignment.Center,
    };
}

public sealed class KeySelector : DataTemplateSelector
{
    public DataTemplate? Fixed { get; set; }
    public DataTemplate? Config { get; set; }
    public override DataTemplate? SelectTemplate(object item, DependencyObject container)
        => item is FixedKey ? Fixed : Config;
}
