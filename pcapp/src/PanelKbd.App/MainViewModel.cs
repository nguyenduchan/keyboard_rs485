using System.IO;
using System.Collections.ObjectModel;
using System.IO.Ports;
using System.Text;
using System.Text.Json;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using Microsoft.Win32;
using PanelKbd.Core;

namespace PanelKbd.App;

public sealed record PortChoice(string Name, bool IsSimulated)
{
    public override string ToString() => Name;
}

public sealed partial class MainViewModel : ObservableObject
{
    // Tab: 0 Thiết kế, 1 Kết nối PLC, 2 Bảo mật & RUN, 3 Kiểm tra, 4 Nạp xuống
    public const int TabDesign = 0, TabCheck = 3, TabDownload = 4;

    static readonly HashSet<string> UiOnly = new()
    {
        "IsSelected", "ResolvedText", "ResolvedError", "RowName", "Number", "Title", "Preview", "DigitCount",
        "IsNumeric", "IsInput", "IsLabelType", "IsBitState", "HasAddress", "CanSpan8", "LowerVisible",
        "NeedsAddress", "NeedsConst", "NeedsPage", "IsBitAction", "IsMaster", "IsUpper",
    };

    readonly DispatcherTimer _debounce;
    readonly SimulatedDevice _sim = new();
    readonly StringBuilder _log = new();

    ProjectFile _project = null!;
    string? _filePath;
    bool _dirty;
    Page? _selectedPage;
    object? _selected;
    int _selectedTab;
    CompileResult? _last;
    bool _busy; int _progress; string _deviceText = "Chưa đọc thiết bị"; PortChoice? _port;

    public ObservableCollection<PlcProfile> Profiles { get; } = new();
    public ObservableCollection<Issue> Issues { get; } = new();
    public ObservableCollection<AddressRow> AddressMap { get; } = new();
    public ObservableCollection<PortChoice> Ports { get; } = new();
    public List<string> ProfileErrors { get; } = new();

    public static readonly List<EnumItem> CellTypeItems = new()
    {
        new(CellType.None, "(trống)"), new(CellType.Display, "Hiện số"), new(CellType.Input, "Nhập số"),
        new(CellType.Label, "Nhãn tĩnh"), new(CellType.BitState, "Trạng thái bit (On/OFF)"),
    };
    public static readonly List<EnumItem> DataTypeItems = new()
    {
        new(DataType.Int16, "int16 (có dấu, 1 thanh ghi)"), new(DataType.UInt16, "uint16 (1 thanh ghi)"),
        new(DataType.Int32, "int32 (2 thanh ghi)"), new(DataType.UInt32, "uint32 (2 thanh ghi)"),
    };
    public static readonly List<EnumItem> PasswordItems = new() { new(0, "Không"), new(1, "Vận hành"), new(2, "Kỹ thuật") };
    public static readonly List<EnumItem> DecimalItems = new() { new(0, "0"), new(1, "1"), new(2, "2"), new(3, "3") };
    public static readonly List<EnumItem> KeyActionItems = new()
    {
        new(KeyAction.None, "(không làm gì)"), new(KeyAction.Hold, "Giữ = 1 (nhả = 0)"), new(KeyAction.Toggle, "Đảo bit"),
        new(KeyAction.Set, "Set (ghi 1)"), new(KeyAction.Reset, "Reset (ghi 0)"), new(KeyAction.WriteConst, "Ghi hằng số"),
        new(KeyAction.GotoPage, "Chuyển tới trang"), new(KeyAction.PrevPage, "Trang trước"), new(KeyAction.NextPage, "Trang sau"),
    };
    public static readonly List<EnumItem> LedModeItems = new()
    {
        new(LedMode.Off, "Tắt"), new(LedMode.Steady, "Sáng theo bit PLC"), new(LedMode.Blink, "Nhấp nháy theo bit PLC"),
        new(LedMode.Comm, "Báo truyền thông (COMM)"),
    };
    public static readonly List<EnumItem> ModeItems = new()
    {
        new(OperationMode.Master, "Master — bàn phím ghi thẳng vào PLC (khuyến nghị)"),
        new(OperationMode.Slave, "Slave — PLC làm master, đọc/ghi bàn phím"),
    };
    public static readonly List<EnumItem> ParityItems = new()
        { new(ParityMode.None, "None"), new(ParityMode.Even, "Even"), new(ParityMode.Odd, "Odd") };
    public static readonly List<EnumItem> BaudItems = new[] { 2400, 4800, 9600, 19200, 38400, 57600, 115200 }
        .Select(b => new EnumItem(b, b.ToString())).ToList();
    public static readonly List<EnumItem> StopItems = new() { new(1, "1"), new(2, "2") };
    public static readonly List<EnumItem> WordOrderItems = new()
    {
        new(WordOrder.ABCD, "ABCD — word cao trước"), new(WordOrder.CDAB, "CDAB — word thấp trước"),
    };
    public static readonly List<EnumItem> StyleItems = new()
    {
        new(AddressStyle.Base0, "Bắt đầu từ 0 (HR0 = 40001)"), new(AddressStyle.Base1, "Bắt đầu từ 1 (HR1 = 40001)"),
        new(AddressStyle.Classic, "Kiểu 40001 / 00001 / 30001 / 10001"),
    };
    public static readonly List<EnumItem> BrightItems = Enumerable.Range(0, 8).Select(i => new EnumItem(i, i.ToString())).ToList();

    public ProjectFile Project
    {
        get => _project;
        private set
        {
            _project = value; Raise();
            foreach (var n in new[] { nameof(KeyF1), nameof(KeyF2), nameof(KeyK3), nameof(KeyUp), nameof(KeyDown), nameof(SelectedProfile), nameof(CurrentProfile), nameof(PageItems), nameof(KeyboardKeys) }) Raise(n);
        }
    }

    public KeyConfig KeyF1 => Project.Keys.First(k => k.Id == "F1");
    public KeyConfig KeyF2 => Project.Keys.First(k => k.Id == "F2");
    public KeyConfig KeyK3 => Project.Keys.First(k => k.Id == "K3");
    public KeyConfig KeyUp => Project.Keys.First(k => k.Id == "UP");
    public KeyConfig KeyDown => Project.Keys.First(k => k.Id == "DN");

    static readonly string[] KeyOrder = { "7", "8", "9", "ESC", "4", "5", "6", "UP", "1", "2", "3", "DN", "F1", "0", "F2", "ENT" };

    /// <summary>16 phím theo đúng vị trí trên bàn phím: FixedKey (cố định) hoặc KeyConfig (gán được).</summary>
    public List<object> KeyboardKeys => KeyOrder.Select(id =>
        (object?)Project.Keys.FirstOrDefault(k => k.Id == id) ?? Project.Layout.FixedKeys.First(k => k.Id == id)).ToList();

    public List<EnumItem> PageItems => Project.Pages.Select((p, i) => new EnumItem(i + 1, p.Title)).ToList();

    public string? FilePath { get => _filePath; private set { if (Set(ref _filePath, value)) RaiseRaw(nameof(WindowTitle)); } }
    public bool Dirty { get => _dirty; private set { if (Set(ref _dirty, value)) RaiseRaw(nameof(WindowTitle)); } }
    public string WindowTitle => $"Panel Keyboard Config — {Project.Name}{(Dirty ? " *" : "")}" + (FilePath != null ? $"  [{FilePath}]" : "");

    public Page? SelectedPage
    {
        get => _selectedPage;
        set
        {
            if (!Set(ref _selectedPage, value) || value == null) return;
            if (Selected is Cell c) Select(c.IsUpper ? value.Upper : value.Lower);
        }
    }

    public object? Selected { get => _selected; private set => Set(ref _selected, value); }
    public int SelectedTab { get => _selectedTab; set { if (Set(ref _selectedTab, value)) OnTabChanged(); } }

    public PlcProfile CurrentProfile => Profiles.FirstOrDefault(p => p.Id == Project.Comm.ProfileId) ?? Profiles.First();
    public PlcProfile SelectedProfile
    {
        get => CurrentProfile;
        set
        {
            if (value == null || value.Id == Project.Comm.ProfileId) return;
            Project.Comm.ProfileId = value.Id;
            Project.Comm.WordOrder = value.DefaultWordOrder;
            Raise(); Raise(nameof(CurrentProfile));
        }
    }

    public int ErrorCount => Issues.Count(i => i.Severity == Severity.Error);
    public int WarningCount => Issues.Count(i => i.Severity == Severity.Warning);
    public int ImageSize => _last?.Image?.Length ?? EstimateSize();
    public double CapacityPercent => Math.Min(100.0, ImageSize * 100.0 / Validator.MaxImageBytes);
    public string CapacityText => $"{ImageSize} / {Validator.MaxImageBytes} B ({CapacityPercent:0}%)";
    public string CheckSummary => $"{ErrorCount} lỗi · {WarningCount} cảnh báo";
    public string CrcText => _last?.Ok == true ? $"CRC32 file chạy: {_last.Crc:X8}" : "Chưa biên dịch được (còn lỗi)";

    public bool IsBusy { get => _busy; private set { if (Set(ref _busy, value)) CommandManager.InvalidateRequerySuggested(); } }
    public int Progress { get => _progress; private set => Set(ref _progress, value); }
    public string DeviceText { get => _deviceText; private set => Set(ref _deviceText, value); }
    public PortChoice? SelectedPort { get => _port; set => Set(ref _port, value); }
    public string LogText => _log.ToString();
    int _cfgStation = 247, _cfgBaud = 9600; ParityMode _cfgParity = ParityMode.None;
    public ParityMode CfgParity { get => _cfgParity; set => Set(ref _cfgParity, value); }
    /// <summary>Số trạm dùng khi cấu hình: 247 ở cửa sổ 3 s sau cấp nguồn / menu PC; địa chỉ riêng nếu dùng ENTER_CONFIG.</summary>
    public int CfgStation { get => _cfgStation; set => Set(ref _cfgStation, value); }
    public int CfgBaud { get => _cfgBaud; set => Set(ref _cfgBaud, value); }
    public ICommand HandshakeCmd { get; private set; } = null!;
    public ICommand EnterConfigCmd { get; private set; } = null!;
    public ICommand BackupCmd { get; private set; } = null!;
    public ICommand DiagCmd { get; private set; } = null!;
    public ICommand ClearDiagCmd { get; private set; } = null!;

    // --- Lệnh ---
    public ICommand NewCmd { get; }
    public ICommand OpenCmd { get; }
    public ICommand SaveCmd { get; }
    public ICommand SaveAsCmd { get; }
    public ICommand AddPageCmd { get; }
    public ICommand DuplicatePageCmd { get; }
    public ICommand DeletePageCmd { get; }
    public ICommand MovePageUpCmd { get; }
    public ICommand MovePageDownCmd { get; }
    public ICommand SelectCmd { get; }
    public ICommand ClearCellCmd { get; }
    public ICommand RefreshPortsCmd { get; }
    public ICommand ReadInfoCmd { get; }
    public ICommand IdentifyCmd { get; }
    public ICommand DetectCmd { get; }
    public ICommand DownloadCmd { get; }
    public ICommand ExitConfigCmd { get; }
    public ICommand FactoryCmd { get; }
    public ICommand ExportBinCmd { get; }
    public ICommand ExportCsvCmd { get; }
    public ICommand GotoIssueCmd { get; }
    public ICommand ClearLogCmd { get; }

    public MainViewModel()
    {
        var exeDir = Path.Combine(AppContext.BaseDirectory, "profiles");
        var userDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "PanelKbdConfig", "profiles");
        var (profs, errs) = PlcProfile.LoadFolders(exeDir, userDir);
        if (profs.Count == 0)
            profs.Add(new PlcProfile { Id = "generic", Name = "Modbus chung", IsGeneric = true, Verified = true,
                Areas = { new PlcArea { Name = "HR", Max = 65535 }, new PlcArea { Name = "C", Kind = AreaKind.Bit, Table = ModbusTable.Coil, Max = 65535 } } });
        foreach (var p in profs) Profiles.Add(p);
        ProfileErrors.AddRange(errs);
        foreach (var e in errs) AppendLog("Hồ sơ lỗi: " + e);

        _debounce = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(150) };
        _debounce.Tick += (_, _) => { _debounce.Stop(); Recompute(); };

        NewCmd = new RelayCommand(NewProject);
        OpenCmd = new RelayCommand(OpenDialog);
        SaveCmd = new RelayCommand(() => Save(false));
        SaveAsCmd = new RelayCommand(() => Save(true));
        AddPageCmd = new RelayCommand(AddPage, () => Project.Pages.Count < ProjectFile.MaxPages);
        DuplicatePageCmd = new RelayCommand(DuplicatePage, () => SelectedPage != null && Project.Pages.Count < ProjectFile.MaxPages);
        DeletePageCmd = new RelayCommand(DeletePage, () => SelectedPage != null && Project.Pages.Count > 1);
        MovePageUpCmd = new RelayCommand(() => MovePage(-1), () => SelectedPage != null && Project.Pages.IndexOf(SelectedPage) > 0);
        MovePageDownCmd = new RelayCommand(() => MovePage(1), () => SelectedPage != null && Project.Pages.IndexOf(SelectedPage) < Project.Pages.Count - 1);
        SelectCmd = new RelayCommand(o => Select(o));
        ClearCellCmd = new RelayCommand(() => { if (Selected is Cell c) c.Type = CellType.None; }, () => Selected is Cell);
        RefreshPortsCmd = new RelayCommand(RefreshPorts);
        ReadInfoCmd = new RelayCommand(() => RunDevice("Đọc thông tin", cli => { var i = cli.ReadInfo(); Ui(() => DeviceText = i.ToString()); return i.ToString(); }), () => !IsBusy && SelectedPort != null);
        IdentifyCmd = new RelayCommand(() => RunDevice("Nhận diện", cli => { cli.Identify(); return "Bàn phím đang bíp và nháy"; }), () => !IsBusy && SelectedPort != null);
        DetectCmd = new RelayCommand(Detect, () => !IsBusy);
        DownloadCmd = new RelayCommand(Download, () => !IsBusy && SelectedPort != null);
        ExitConfigCmd = new RelayCommand(() => RunDevice("Thoát chế độ cấu hình", cli => { cli.Exit(); return "Đã gửi EXIT — bàn phím khởi động lại sau ~80 ms và chạy theo cấu hình"; }), () => !IsBusy && SelectedPort != null);
        FactoryCmd = new RelayCommand(Factory, () => !IsBusy && SelectedPort != null);
        HandshakeCmd = new RelayCommand(() => RunDevice("Bắt tay — cấp nguồn lại bàn phím ngay (cửa sổ 3 giây đầu)", cli =>
        {
            var i = cli.Handshake(30000); Ui(() => DeviceText = i.ToString()); return "Bắt tay xong: " + i;
        }), () => !IsBusy && SelectedPort != null);
        EnterConfigCmd = new RelayCommand(() => RunDevice("Vào chế độ cấu hình (ENTER_CONFIG)", cli =>
        {
            cli.EnterConfigMode(); var i = cli.ReadInfo(); Ui(() => DeviceText = i.ToString()); return "Đã ở chế độ cấu hình: " + i;
        }), () => !IsBusy && SelectedPort != null);
        BackupCmd = new RelayCommand(Backup, () => !IsBusy && SelectedPort != null);
        DiagCmd = new RelayCommand(() => RunDevice("Đọc chẩn đoán", cli => cli.ReadDiagnostics().ToString()), () => !IsBusy && SelectedPort != null);
        ClearDiagCmd = new RelayCommand(() => RunDevice("Xoá bộ đếm chẩn đoán", cli => { cli.ClearDiagnostics(); return "Đã xoá"; }), () => !IsBusy && SelectedPort != null);
        ExportBinCmd = new RelayCommand(ExportBin);
        ExportCsvCmd = new RelayCommand(ExportCsv);
        GotoIssueCmd = new RelayCommand(o => { if (o is Issue i) GotoIssue(i); });
        ClearLogCmd = new RelayCommand(() => { _log.Clear(); Raise(nameof(LogText)); });

        RefreshPorts();
        InitSim();
        LoadProject(ProjectFile.CreateNew(), null);
    }

    // --- Dự án ---
    void LoadProject(ProjectFile p, string? path)
    {
        Project = p;
        Hook(p);
        FilePath = path;
        SelectedPage = p.Pages.FirstOrDefault();
        Select(SelectedPage?.Upper);
        Recompute();
        Dirty = false;
        Raise(nameof(WindowTitle));
        SimProjectChanged();
    }

    public bool ConfirmDiscard()
    {
        if (!Dirty) return true;
        var r = MessageBox.Show($"Lưu thay đổi của dự án '{Project.Name}'?", "Panel Keyboard Config",
            MessageBoxButton.YesNoCancel, MessageBoxImage.Question);
        if (r == MessageBoxResult.Cancel) return false;
        if (r == MessageBoxResult.Yes) return Save(false);
        return true;
    }

    void NewProject() { if (ConfirmDiscard()) LoadProject(ProjectFile.CreateNew(), null); }

    void OpenDialog()
    {
        if (!ConfirmDiscard()) return;
        var d = new OpenFileDialog { Filter = "Dự án bàn phím (*.pkproj)|*.pkproj|Mọi file|*.*" };
        if (d.ShowDialog() == true) Open(d.FileName);
    }

    public void Open(string path)
    {
        try { LoadProject(ProjectStorage.Load(path), path); }
        catch (Exception ex) { MessageBox.Show("Không mở được dự án:\n" + ex.Message, "Lỗi", MessageBoxButton.OK, MessageBoxImage.Error); }
    }

    bool Save(bool saveAs)
    {
        var path = FilePath;
        if (saveAs || path == null)
        {
            var d = new SaveFileDialog { Filter = "Dự án bàn phím (*.pkproj)|*.pkproj", FileName = Project.Name + ProjectStorage.Extension };
            if (d.ShowDialog() != true) return false;
            path = d.FileName;
        }
        try { ProjectStorage.Save(Project, path); }
        catch (Exception ex) { MessageBox.Show("Không lưu được:\n" + ex.Message, "Lỗi", MessageBoxButton.OK, MessageBoxImage.Error); return false; }
        FilePath = path; Dirty = false;
        return true;
    }

    // --- Theo dõi thay đổi ---
    void Hook(ProjectFile p)
    {
        p.PropertyChanged += OnAnyChanged;
        p.Comm.PropertyChanged += OnAnyChanged;
        p.Security.PropertyChanged += OnAnyChanged;
        p.Layout.PropertyChanged += OnAnyChanged;
        foreach (var fk in p.Layout.FixedKeys) fk.PropertyChanged += OnAnyChanged;
        foreach (var k in p.Keys) k.PropertyChanged += OnAnyChanged;
        foreach (var l in p.Leds) l.PropertyChanged += OnAnyChanged;
        foreach (var pg in p.Pages) HookPage(pg);
        p.Pages.CollectionChanged += (_, e) =>
        {
            if (e.NewItems != null) foreach (Page pg in e.NewItems) HookPage(pg);
            p.RenumberPages(); Raise(nameof(PageItems)); Touch();
        };
    }

    void HookPage(Page pg)
    {
        pg.PropertyChanged += OnAnyChanged; pg.Upper.PropertyChanged += OnAnyChanged; pg.Lower.PropertyChanged += OnAnyChanged;
    }

    void OnAnyChanged(object? s, System.ComponentModel.PropertyChangedEventArgs e)
    {
        if (e.PropertyName != null && UiOnly.Contains(e.PropertyName)) return;
        if (s is Page && e.PropertyName is "Upper" or "Lower") return;
        if (s is Page && e.PropertyName == nameof(Page.Name)) Raise(nameof(PageItems));
        if (s is ProjectFile) RaiseRaw(nameof(WindowTitle));
        if (s is CommSettings && e.PropertyName == nameof(CommSettings.ProfileId)) { Raise(nameof(CurrentProfile)); Raise(nameof(SelectedProfile)); }
        Touch();
    }

    void Touch() { Dirty = true; _debounce.Stop(); _debounce.Start(); }

    void RefreshAbsoluteAddresses()
    {
        AddressContext.Profile = CurrentProfile; AddressContext.Comm = Project.Comm;
        foreach (var pg in Project.Pages) { pg.Upper.RefreshAbsolute(); pg.Lower.RefreshAbsolute(); }
        foreach (var k in Project.Keys) k.RefreshAbsolute();
        foreach (var l in Project.Leds) l.RefreshAbsolute();
        foreach (var w in Watch) w.RefreshAbsolute();
    }

    public void Recompute()
    {
        RefreshAbsoluteAddresses();
        _last = RuntimeCompiler.Compile(Project, CurrentProfile);
        Issues.Clear();
        foreach (var i in _last.Check.Issues.OrderByDescending(x => x.Severity)) Issues.Add(i);
        AddressMap.Clear();
        foreach (var r in _last.Check.Map) AddressMap.Add(r);
        SimProjectChanged();
        foreach (var n in new[] { nameof(ErrorCount), nameof(WarningCount), nameof(ImageSize), nameof(CapacityPercent), nameof(CapacityText), nameof(CheckSummary), nameof(CrcText) }) Raise(n);
    }

    int EstimateSize() => RuntimeCompiler.ImageSize;

    // --- Chọn / trang ---
    public void Select(object? o)
    {
        if (Selected is Cell c0) c0.IsSelected = false;
        else if (Selected is KeyConfig k0) k0.IsSelected = false;
        else if (Selected is LedConfig l0) l0.IsSelected = false;
        Selected = o;
        if (o is Cell c) c.IsSelected = true;
        else if (o is KeyConfig k) k.IsSelected = true;
        else if (o is LedConfig l) l.IsSelected = true;
    }

    void AddPage()
    {
        var pg = new Page { Upper = new Cell { Type = CellType.Label, Label = "PAGE" }, Lower = new Cell { Type = CellType.None } };
        Project.Pages.Add(pg); SelectedPage = pg;
    }

    void DuplicatePage()
    {
        if (SelectedPage == null) return;
        var copy = JsonSerializer.Deserialize<Page>(JsonSerializer.Serialize(SelectedPage))!;
        Project.Pages.Insert(Project.Pages.IndexOf(SelectedPage) + 1, copy);
        SelectedPage = copy;
    }

    void DeletePage()
    {
        if (SelectedPage == null) return;
        int i = Project.Pages.IndexOf(SelectedPage);
        Project.Pages.Remove(SelectedPage);
        SelectedPage = Project.Pages[Math.Min(i, Project.Pages.Count - 1)];
    }

    void MovePage(int d)
    {
        if (SelectedPage == null) return;
        int i = Project.Pages.IndexOf(SelectedPage);
        var keep = SelectedPage;
        Project.Pages.Move(i, i + d);
        SelectedPage = keep;
    }

    void GotoIssue(Issue i)
    {
        if (i.Target is Cell c)
        {
            var pg = Project.Pages.FirstOrDefault(p => ReferenceEquals(p.Upper, c) || ReferenceEquals(p.Lower, c));
            if (pg != null) { SelectedPage = pg; Select(c); SelectedTab = TabDesign; }
        }
        else if (i.Target is KeyConfig or LedConfig) { Select(i.Target); SelectedTab = TabDesign; }
        else if (i.Target is Page p2) { SelectedPage = p2; SelectedTab = TabDesign; }
        else if (i.Where.StartsWith("Bảo mật")) SelectedTab = 2;
        else if (i.Where.StartsWith("Kết nối") || i.Where.StartsWith("Hồ sơ")) SelectedTab = 1;
    }

    // --- Xuất ---
    void ExportBin()
    {
        Recompute();
        if (_last?.Image == null) { Fail("Còn lỗi, chưa tạo được file chạy. Xem tab Kiểm tra."); SelectedTab = TabCheck; return; }
        var d = new SaveFileDialog { Filter = "File chạy bàn phím (*.pkb)|*.pkb", FileName = Project.Name + ".pkb" };
        if (d.ShowDialog() != true) return;
        File.WriteAllBytes(d.FileName, _last.Image);
        AppendLog($"Đã xuất {d.FileName} ({_last.Image.Length} B, CRC32 {_last.Crc:X8})");
    }

    void ExportCsv()
    {
        Recompute();
        var d = new SaveFileDialog { Filter = "CSV (*.csv)|*.csv", FileName = Project.Name + "_dia_chi.csv" };
        if (d.ShowDialog() != true) return;
        var sb = new StringBuilder("Vị trí,Vai trò,Địa chỉ PLC,Bảng Modbus,Modbus (dec),Modbus (hex),Kiểu 4xxxx,Kiểu dữ liệu\r\n");
        foreach (var r in AddressMap)
            sb.AppendLine(string.Join(',', new[] { r.Where, r.Role, r.Plc, r.Table, r.Modbus.ToString(), r.Hex, r.Classic, r.Type }.Select(Csv)));
        File.WriteAllText(d.FileName, sb.ToString(), new UTF8Encoding(true));
        AppendLog("Đã xuất bảng địa chỉ " + d.FileName);
    }

    static string Csv(string s) => s.Contains(',') || s.Contains('"') ? "\"" + s.Replace("\"", "\"\"") + "\"" : s;

    // --- Thiết bị ---
    void RefreshPorts()
    {
        var keep = SelectedPort?.Name;
        Ports.Clear();
        foreach (var n in SerialPort.GetPortNames().OrderBy(x => x.Length).ThenBy(x => x)) Ports.Add(new PortChoice(n, false));
        Ports.Add(new PortChoice(_sim.Name, true));
        SelectedPort = Ports.FirstOrDefault(p => p.Name == keep) ?? Ports.First();
    }

    IModbusPort CreatePort(PortChoice c) => c.IsSimulated ? _sim : new SerialModbusPort(c.Name, CfgBaud, CfgParity switch { ParityMode.Even => Parity.Even, ParityMode.Odd => Parity.Odd, _ => Parity.None });

    void Ui(Action a) => Application.Current.Dispatcher.BeginInvoke(a);

    void AppendLog(string s)
    {
        _log.AppendLine($"{DateTime.Now:HH:mm:ss}  {s}");
        Raise(nameof(LogText));
    }

    void Fail(string msg) { AppendLog("✖ " + msg); }

    ConfigClient NewClient(IModbusPort port)
    {
        var cli = new ConfigClient(port) { Station = (byte)Math.Clamp(CfgStation, 1, 247) };
        cli.Frame += (tx, f) => Ui(() => AppendLog((tx ? "TX " : "RX ") + ModbusRtu.Hex(f)));
        return cli;
    }

    void RunDevice(string title, Func<ConfigClient, string> work)
    {
        var choice = SelectedPort; if (choice == null) return;
        IsBusy = true; Progress = 0; AppendLog($"▶ {title} ({choice.Name})");
        Task.Run(() =>
        {
            try
            {
                using var port = CreatePort(choice);
                port.Open();
                try { return (ok: true, msg: work(NewClient(port))); }
                finally { port.Close(); }
            }
            catch (Exception ex) { return (ok: false, msg: ex.Message); }
        }).ContinueWith(t => Ui(() =>
        {
            var (ok, msg) = t.Result;
            AppendLog((ok ? "✔ " : "✖ ") + msg);
            IsBusy = false;
        }));
    }

    void Download()
    {
        Recompute();
        if (_last?.Image == null)
        {
            Fail("Còn lỗi, chưa gửi được. Xem tab Kiểm tra.");
            SelectedTab = TabCheck; return;
        }
        if (!CurrentProfile.Verified && Project.Comm.Mode == OperationMode.Master)
            AppendLog($"⚠ Hồ sơ '{CurrentProfile.Name}' chưa kiểm tra bằng PLC thật.");
        // revision tăng mỗi lần nạp (để biết bản nào đang chạy); nằm trong vùng tính CRC nên biên dịch lại.
        var rev = RuntimeCompiler.Compile(Project, CurrentProfile, (int)(DateTimeOffset.UtcNow.ToUnixTimeSeconds() & 0xFFFF));
        var image = rev.Image ?? _last.Image;
        var prog = new Progress<int>(v => Progress = v);
        RunDevice("Gửi cấu hình", cli =>
        {
            cli.Download(image, prog);
            return $"Đã gửi cấu hình {image.Length} B, đọc lại khớp, đã ÁP DỤNG (CRC32 {Crc32.Compute(image):X8})";
        });
    }

    void Backup()
    {
        var d = new SaveFileDialog { Filter = "File chạy bàn phím (*.pkb)|*.pkb", FileName = Project.Name + "_backup.pkb" };
        if (d.ShowDialog() != true) return;
        var path = d.FileName;
        var prog = new Progress<int>(p => Progress = p);
        RunDevice("Sao lưu cấu hình từ bàn phím", cli =>
        {
            var img = cli.ReadConfig(prog);
            File.WriteAllBytes(path, img);
            return $"Đã lưu {img.Length} B vào {path} (CRC32 {Crc32.Compute(img.AsSpan(0, RuntimeCompiler.CrcOffset)):X8})";
        });
    }

    void Factory()
    {
        if (MessageBox.Show("Đưa bàn phím về mặc định nhà máy? Cấu hình trên thiết bị sẽ mất.", "Xác nhận",
                MessageBoxButton.YesNo, MessageBoxImage.Warning) != MessageBoxResult.Yes) return;
        RunDevice("Mặc định nhà máy", cli => { cli.FactoryReset(); return "Đã đặt lại mặc định nhà máy"; });
    }

    void Detect()
    {
        var cands = Ports.Where(p => !p.IsSimulated).ToList();
        if (cands.Count == 0) { Fail("Không thấy cổng COM nào. Cắm USB–RS485 rồi bấm 'Làm mới'."); return; }
        IsBusy = true; AppendLog("▶ Tự tìm bàn phím ở chế độ cấu hình (trạm 247, 9600 8N1)…");
        Task.Run(() =>
        {
            foreach (var c in cands)
            {
                try
                {
                    using var port = new SerialModbusPort(c.Name);
                    port.Open();
                    var cli = new ConfigClient(port) { TimeoutMs = 150, Retries = 0 };
                    var info = cli.ReadInfo();
                    port.Close();
                    return (c, info.ToString());
                }
                catch { /* thử cổng tiếp */ }
            }
            return ((PortChoice?)null, "");
        }).ContinueWith(t => Ui(() =>
        {
            var (c, text) = t.Result;
            if (c != null) { SelectedPort = c; DeviceText = text; AppendLog($"✔ Thấy bàn phím ở {c.Name}: {text}"); }
            else AppendLog("✖ Không thấy bàn phím. Vào menu 'PC' trên máy hoặc cấp nguồn lại rồi thử trong 3 s đầu.");
            IsBusy = false;
        }));
    }
}
