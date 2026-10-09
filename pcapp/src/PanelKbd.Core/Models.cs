using System.Collections.ObjectModel;
using System.Text.Json.Serialization;

namespace PanelKbd.Core;

public enum CellType { None, Display, Input, Label, BitState }
public enum DataType { Int16, UInt16, Int32, UInt32 }
public enum WordOrder { ABCD, CDAB }
public enum KeyAction { None, Hold, Toggle, Set, Reset, WriteConst, GotoPage, PrevPage, NextPage }
public enum LedMode { Off, Steady, Blink, Comm }
public enum ParityMode { None, Even, Odd }
public enum OperationMode { Master, Slave }
public enum AddressStyle { Base0, Base1, Classic }

public static class DataTypeExt
{
    public static int Registers(this DataType t) => t is DataType.Int32 or DataType.UInt32 ? 2 : 1;
}

/// <summary>Một đối tượng trên hàng số (hiện số, nhập số, nhãn, trạng thái bit).</summary>
public sealed class Cell : ObservableObject
{
    CellType _type; string _address = ""; DataType _dataType = DataType.Int16; int _decimals;
    int _min; int _max = 9999; string _label = ""; string _onLabel = "On"; string _offLabel = "OFF";
    int _password; bool _span8; string _note = "";
    bool _isUpper; bool _isSelected; string _rowName = ""; string _resolvedText = ""; bool _resolvedError;

    public CellType Type { get => _type; set => Set(ref _type, value); }
    public string Address { get => _address; set => Set(ref _address, value ?? ""); }
    /// <summary>Địa chỉ Modbus tuyệt đối (HR:4096…) của cùng ô địa chỉ; sửa ô nào thì ô kia đổi theo.</summary>
    [JsonIgnore] public string AbsoluteAddress
    {
        get => AddressContext.ToAbsolute(Address);
        set { var s = AddressContext.FromAbsolute(value); if (s != Address) Address = s; else RaiseRaw(nameof(AbsoluteAddress)); }
    }
    public void RefreshAbsolute() => RaiseRaw(nameof(AbsoluteAddress));
    public DataType DataType { get => _dataType; set => Set(ref _dataType, value); }
    public int Decimals { get => _decimals; set => Set(ref _decimals, Math.Clamp(value, 0, 3)); }
    public int Min { get => _min; set => Set(ref _min, value); }
    public int Max { get => _max; set => Set(ref _max, value); }
    public string Label { get => _label; set => Set(ref _label, value ?? ""); }
    public string OnLabel { get => _onLabel; set => Set(ref _onLabel, value ?? ""); }
    public string OffLabel { get => _offLabel; set => Set(ref _offLabel, value ?? ""); }
    /// <summary>0 = không, 1 = vận hành, 2 = kỹ thuật.</summary>
    public int PasswordLevel { get => _password; set => Set(ref _password, Math.Clamp(value, 0, 2)); }
    /// <summary>Chỉ áp dụng hàng trên: ghép 8 số, hàng dưới bị che.</summary>
    public bool Span8 { get => _span8; set => Set(ref _span8, value); }
    public string Note { get => _note; set => Set(ref _note, value ?? ""); }

    [JsonIgnore] public bool IsUpper { get => _isUpper; set => Set(ref _isUpper, value); }
    [JsonIgnore] public bool IsSelected { get => _isSelected; set => Set(ref _isSelected, value); }
    [JsonIgnore] public string RowName { get => _rowName; set => Set(ref _rowName, value); }
    [JsonIgnore] public string ResolvedText { get => _resolvedText; set => Set(ref _resolvedText, value); }
    [JsonIgnore] public bool ResolvedError { get => _resolvedError; set => Set(ref _resolvedError, value); }

    [JsonIgnore] public int DigitCount => IsUpper && Span8 ? 8 : 4;
    [JsonIgnore] public bool IsNumeric => Type is CellType.Display or CellType.Input;
    [JsonIgnore] public bool IsInput => Type == CellType.Input;
    [JsonIgnore] public bool IsLabelType => Type == CellType.Label;
    [JsonIgnore] public bool IsBitState => Type == CellType.BitState;
    [JsonIgnore] public bool HasAddress => Type is CellType.Display or CellType.Input or CellType.BitState;
    [JsonIgnore] public bool CanSpan8 => IsUpper;

    [JsonIgnore]
    public string Preview
    {
        get
        {
            switch (Type)
            {
                case CellType.Label: return Label;
                case CellType.BitState: return OnLabel;
                case CellType.Display:
                case CellType.Input:
                    int n = DigitCount;
                    var s = "12345678".Substring(0, n);
                    if (Decimals > 0 && Decimals < n) s = s.Insert(n - Decimals, ".");
                    return s;
                default: return "";
            }
        }
    }

    static readonly string[] Derived =
    {
        nameof(Preview), nameof(DigitCount), nameof(IsNumeric), nameof(IsInput),
        nameof(IsLabelType), nameof(IsBitState), nameof(HasAddress), nameof(CanSpan8),
    };

    protected override void OnChanged(string name)
    {
        base.OnChanged(name);
        if (name == nameof(Address)) RaiseRaw(nameof(AbsoluteAddress));
        if (name is nameof(IsSelected) or nameof(ResolvedText) or nameof(ResolvedError) or nameof(RowName)) return;
        foreach (var n in Derived) RaiseRaw(n);
    }
}

public sealed class Page : ObservableObject
{
    string _name = ""; int _number;
    Cell _upper = new();
    Cell _lower = new();

    public string Name { get => _name; set { if (Set(ref _name, value ?? "")) RaiseRaw(nameof(Title)); } }

    public Cell Upper
    {
        get => _upper;
        set
        {
            _upper = value;
            value.IsUpper = true;
            value.PropertyChanged += (_, e) => { if (e.PropertyName == nameof(Cell.Span8)) RaiseRaw(nameof(LowerVisible)); };
            Raise();
            RaiseRaw(nameof(LowerVisible));
        }
    }

    public Cell Lower { get => _lower; set { _lower = value; value.IsUpper = false; Raise(); } }

    [JsonIgnore] public int Number { get => _number; set { if (Set(ref _number, value)) RaiseRaw(nameof(Title)); } }
    [JsonIgnore] public bool LowerVisible => !Upper.Span8;
    [JsonIgnore] public string Title => string.IsNullOrWhiteSpace(Name) ? $"Trang {Number}" : $"Trang {Number} - {Name}";

    public Page()
    {
        Upper = _upper;
        Lower = _lower;
    }
}

public sealed class KeyConfig : ObservableObject
{
    string _id = ""; string _title = ""; KeyAction _action; string _address = ""; int _const; int _targetPage = 1; string _note = "";
    bool _isSelected; string _resolvedText = ""; bool _resolvedError; bool _present = true;

    /// <summary>Board có lắp phím này không.</summary>
    public bool Present { get => _present; set => Set(ref _present, value); }
    public string Id { get => _id; set => Set(ref _id, value); }
    public string Title { get => _title; set => Set(ref _title, value); }
    public KeyAction Action { get => _action; set => Set(ref _action, value); }
    public string Address { get => _address; set => Set(ref _address, value ?? ""); }
    /// <summary>Địa chỉ Modbus tuyệt đối (HR:4096…) của cùng ô địa chỉ; sửa ô nào thì ô kia đổi theo.</summary>
    [JsonIgnore] public string AbsoluteAddress
    {
        get => AddressContext.ToAbsolute(Address);
        set { var s = AddressContext.FromAbsolute(value); if (s != Address) Address = s; else RaiseRaw(nameof(AbsoluteAddress)); }
    }
    public void RefreshAbsolute() => RaiseRaw(nameof(AbsoluteAddress));
    /// <summary>Hằng số ghi cho KeyAction.WriteConst (ghi 16 bit).</summary>
    public int Const { get => _const; set => Set(ref _const, value); }
    public int TargetPage { get => _targetPage; set => Set(ref _targetPage, value); }
    public string Note { get => _note; set => Set(ref _note, value ?? ""); }

    [JsonIgnore] public bool IsSelected { get => _isSelected; set => Set(ref _isSelected, value); }
    [JsonIgnore] public string ResolvedText { get => _resolvedText; set => Set(ref _resolvedText, value); }
    [JsonIgnore] public bool ResolvedError { get => _resolvedError; set => Set(ref _resolvedError, value); }
    [JsonIgnore] public bool NeedsAddress => Action is KeyAction.Hold or KeyAction.Toggle or KeyAction.Set or KeyAction.Reset or KeyAction.WriteConst;
    [JsonIgnore] public bool NeedsConst => Action == KeyAction.WriteConst;
    [JsonIgnore] public bool NeedsPage => Action == KeyAction.GotoPage;
    [JsonIgnore] public bool IsBitAction => Action is KeyAction.Hold or KeyAction.Toggle or KeyAction.Set or KeyAction.Reset;

    protected override void OnChanged(string name)
    {
        base.OnChanged(name);
        if (name == nameof(Address)) RaiseRaw(nameof(AbsoluteAddress));
        if (name is nameof(IsSelected) or nameof(ResolvedText) or nameof(ResolvedError)) return;
        RaiseRaw(nameof(NeedsAddress)); RaiseRaw(nameof(NeedsConst)); RaiseRaw(nameof(NeedsPage)); RaiseRaw(nameof(IsBitAction));
    }
}

public sealed class LedConfig : ObservableObject
{
    int _index; LedMode _mode; string _address = ""; bool _invert; string _note = "";
    bool _isSelected; string _resolvedText = ""; bool _resolvedError; bool _present = true;

    /// <summary>Board có lắp LED này không.</summary>
    public bool Present { get => _present; set => Set(ref _present, value); }
    public int Index { get => _index; set => Set(ref _index, value); }
    public LedMode Mode { get => _mode; set => Set(ref _mode, value); }
    public string Address { get => _address; set => Set(ref _address, value ?? ""); }
    /// <summary>Địa chỉ Modbus tuyệt đối (HR:4096…) của cùng ô địa chỉ; sửa ô nào thì ô kia đổi theo.</summary>
    [JsonIgnore] public string AbsoluteAddress
    {
        get => AddressContext.ToAbsolute(Address);
        set { var s = AddressContext.FromAbsolute(value); if (s != Address) Address = s; else RaiseRaw(nameof(AbsoluteAddress)); }
    }
    public void RefreshAbsolute() => RaiseRaw(nameof(AbsoluteAddress));
    public bool Invert { get => _invert; set => Set(ref _invert, value); }
    public string Note { get => _note; set => Set(ref _note, value ?? ""); }

    [JsonIgnore] public bool IsSelected { get => _isSelected; set => Set(ref _isSelected, value); }
    [JsonIgnore] public string ResolvedText { get => _resolvedText; set => Set(ref _resolvedText, value); }
    [JsonIgnore] public bool ResolvedError { get => _resolvedError; set => Set(ref _resolvedError, value); }
    [JsonIgnore] public bool NeedsAddress => Mode is LedMode.Steady or LedMode.Blink;
    [JsonIgnore] public string Title => $"LED {Index}";

    protected override void OnChanged(string name)
    {
        base.OnChanged(name);
        if (name == nameof(Address)) RaiseRaw(nameof(AbsoluteAddress));
        if (name is nameof(IsSelected) or nameof(ResolvedText) or nameof(ResolvedError)) return;
        RaiseRaw(nameof(NeedsAddress)); RaiseRaw(nameof(Title));
    }
}

public sealed class CommSettings : ObservableObject
{
    string _profileId = "generic"; int _userOffset; AddressStyle _style = AddressStyle.Base0;
    OperationMode _mode = OperationMode.Master; int _plcStation = 1; int _kbStation = 1;
    int _baud = 9600; ParityMode _parity = ParityMode.None; int _stop = 1;
    int _timeout = 200; int _retries = 2; int _poll = 100; bool _readBack = true;
    WordOrder _wordOrder = WordOrder.CDAB; int _brightness = 5; bool _click = true;

    public string ProfileId { get => _profileId; set => Set(ref _profileId, value); }
    /// <summary>Địa chỉ PLC tại đó thanh ghi Modbus 0 nằm (Siemens V, LS...). Chỉ dùng khi hồ sơ yêu cầu.</summary>
    public int UserOffset { get => _userOffset; set => Set(ref _userOffset, value); }
    public AddressStyle AddressStyle { get => _style; set => Set(ref _style, value); }
    public OperationMode Mode { get => _mode; set => Set(ref _mode, value); }
    public int PlcStation { get => _plcStation; set => Set(ref _plcStation, value); }
    public int KeyboardStation { get => _kbStation; set => Set(ref _kbStation, value); }
    public int Baud { get => _baud; set => Set(ref _baud, value); }
    public ParityMode Parity { get => _parity; set => Set(ref _parity, value); }
    public int StopBits { get => _stop; set => Set(ref _stop, value); }
    public int TimeoutMs { get => _timeout; set => Set(ref _timeout, value); }
    public int Retries { get => _retries; set => Set(ref _retries, value); }
    public int PollMs { get => _poll; set => Set(ref _poll, value); }
    public bool ReadBackAfterWrite { get => _readBack; set => Set(ref _readBack, value); }
    public WordOrder WordOrder { get => _wordOrder; set => Set(ref _wordOrder, value); }
    public int Brightness { get => _brightness; set => Set(ref _brightness, value); }
    public bool BuzzerClick { get => _click; set => Set(ref _click, value); }

    [JsonIgnore] public bool IsMaster => Mode == OperationMode.Master;
    protected override void OnChanged(string name) { base.OnChanged(name); RaiseRaw(nameof(IsMaster)); }
}

public sealed class SecuritySettings : ObservableObject
{
    string _op = ""; string _tech = ""; int _auto = 60; int _runPage = 1; int _runTimeout = 30;
    /// <summary>1–4 chữ số, rỗng = không đặt.</summary>
    public string OperatorPassword { get => _op; set => Set(ref _op, value ?? ""); }
    public string TechPassword { get => _tech; set => Set(ref _tech, value ?? ""); }
    public int AutoLogoutSec { get => _auto; set => Set(ref _auto, value); }
    /// <summary>Trang RUN (đánh số từ 1).</summary>
    public int RunPage { get => _runPage; set => Set(ref _runPage, value); }
    /// <summary>Không bấm phím quá N giây thì quay về trang RUN; 0 = tắt.</summary>
    public int RunTimeoutSec { get => _runTimeout; set => Set(ref _runTimeout, value); }
}

/// <summary>Phím cố định (số, ESC, ENT) — chỉ để khai báo board có lắp hay không.</summary>
public sealed class FixedKey : ObservableObject
{
    string _id = ""; bool _present = true;
    public string Id { get => _id; set => Set(ref _id, value); }
    public bool Present { get => _present; set => Set(ref _present, value); }
    [JsonIgnore] public string Title => Id;
}

/// <summary>Bố cục phần cứng: board thực tế không phải lúc nào cũng đủ phím/LED/hàng số.</summary>
public sealed class LayoutConfig : ObservableObject
{
    bool _upper = true, _lower = true;
    public bool HasUpper { get => _upper; set => Set(ref _upper, value); }
    public bool HasLower { get => _lower; set => Set(ref _lower, value); }
    public ObservableCollection<FixedKey> FixedKeys { get; set; } = new();
    public static readonly string[] FixedIds = { "7", "8", "9", "ESC", "4", "5", "6", "1", "2", "3", "0", "ENT" };
    public bool IsPresent(string id) => FixedKeys.FirstOrDefault(k => k.Id == id)?.Present ?? true;
}

public sealed class ProjectFile : ObservableObject
{
    public const int CurrentFormat = 1;
    public const int MaxPages = 32;

    string _name = "Dự án mới"; string _notes = ""; string _model = "PK-8D8L-485"; string _fw = "1.0";

    public int FormatVersion { get; set; } = CurrentFormat;
    public string Name { get => _name; set => Set(ref _name, value ?? ""); }
    public string Notes { get => _notes; set => Set(ref _notes, value ?? ""); }
    public string Model { get => _model; set => Set(ref _model, value ?? ""); }
    public string TargetFirmware { get => _fw; set => Set(ref _fw, value ?? ""); }
    public CommSettings Comm { get; set; } = new();
    public SecuritySettings Security { get; set; } = new();
    public LayoutConfig Layout { get; set; } = new();
    public ObservableCollection<Page> Pages { get; set; } = new();
    public ObservableCollection<KeyConfig> Keys { get; set; } = new();
    public ObservableCollection<LedConfig> Leds { get; set; } = new();

    public static readonly (string Id, string Title)[] KeyIds =
        { ("F1", "F1"), ("F2", "F2"), ("K3", "K3"), ("UP", "▲"), ("DN", "▼") };

    public static ProjectFile CreateNew()
    {
        var p = new ProjectFile();
        p.Pages.Add(new Page
        {
            Name = "RUN",
            Upper = new Cell { Type = CellType.Display, Address = "HR100", Decimals = 1 },
            Lower = new Cell { Type = CellType.Input, Address = "HR101", Decimals = 1, Min = 0, Max = 9999 },
        });
        p.EnsureComplete();
        return p;
    }

    /// <summary>Bổ sung phím/LED còn thiếu (▲▼ mặc định = chuyển trang), sắp xếp, đánh số lại trang.</summary>
    public void EnsureComplete()
    {
        foreach (var (id, title) in KeyIds)
        {
            var k = Keys.FirstOrDefault(x => x.Id == id);
            if (k == null)
            {
                k = new KeyConfig { Id = id, Title = title };
                if (id == "UP") k.Action = KeyAction.PrevPage;
                if (id == "DN") k.Action = KeyAction.NextPage;
                Keys.Add(k);
            }
            k.Title = title;
        }
        foreach (var id in LayoutConfig.FixedIds)
            if (Layout.FixedKeys.All(k => k.Id != id)) Layout.FixedKeys.Add(new FixedKey { Id = id });
        for (int i = 1; i <= 8; i++)
            if (Leds.All(l => l.Index != i))
                Leds.Add(new LedConfig { Index = i, Mode = i == 8 ? LedMode.Comm : LedMode.Off });
        Reorder(Leds, Leds.OrderBy(l => l.Index).ToList());
        var order = KeyIds.Select(k => k.Id).ToList();
        Reorder(Keys, Keys.OrderBy(k => order.IndexOf(k.Id)).ToList());
        RenumberPages();
    }

    static void Reorder<T>(ObservableCollection<T> col, List<T> sorted)
    {
        for (int i = 0; i < sorted.Count; i++)
            if (!ReferenceEquals(col[i], sorted[i])) col.Move(col.IndexOf(sorted[i]), i);
    }

    public void RenumberPages()
    {
        for (int i = 0; i < Pages.Count; i++)
        {
            Pages[i].Number = i + 1;
            Pages[i].Upper.RowName = $"Trang {i + 1} · hàng trên";
            Pages[i].Lower.RowName = $"Trang {i + 1} · hàng dưới";
        }
    }
}
