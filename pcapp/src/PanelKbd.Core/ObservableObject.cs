using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace PanelKbd.Core;

public abstract class ObservableObject : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;

    protected bool Set<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        Raise(name);
        return true;
    }

    protected void Raise([CallerMemberName] string? name = null) => OnChanged(name ?? "");

    /// <summary>Lớp con ghi đè để báo thêm các thuộc tính phụ thuộc.</summary>
    protected virtual void OnChanged(string name)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    /// <summary>Báo một thuộc tính phụ thuộc mà không gọi lại OnChanged của lớp con.</summary>
    protected void RaiseRaw(string name)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
}
