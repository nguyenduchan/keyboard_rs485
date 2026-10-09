using System.IO;
using System.Windows;

namespace PanelKbd.App;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        DispatcherUnhandledException += (_, ex) =>
        {
            MessageBox.Show(ex.Exception.Message, "Lỗi không mong muốn", MessageBoxButton.OK, MessageBoxImage.Error);
            ex.Handled = true;
        };
        base.OnStartup(e);
        var args = e.Args;
        Dispatcher.BeginInvoke(() => ApplyArgs(args));
    }

    void ApplyArgs(string[] args)
    {
        if (MainWindow?.DataContext is not MainViewModel vm) return;
        // Tham số: [file.pkproj] [--tab N]  (--tab dùng để chụp ảnh kiểm tra giao diện)
        for (int i = 0; i < args.Length; i++)
        {
            if (args[i] == "--tab" && i + 1 < args.Length && int.TryParse(args[i + 1], out var t)) { vm.SelectedTab = t; i++; }
            else if (File.Exists(args[i])) vm.Open(args[i]);
        }
    }
}
