using System.Collections.ObjectModel;
using System.IO;
using System.IO.Ports;
using System.Text;
using System.Windows;
using System.Windows.Input;
using System.Windows.Threading;
using PanelKbd.Core;

namespace PanelKbd.App;

public sealed class SimKey : ObservableObject
{
    bool _present = true, _pressed;
    public string Id { get; init; } = "";
    public string Title { get; init; } = "";
    public bool Configurable { get; init; }
    public bool Present { get => _present; set => Set(ref _present, value); }
    public bool Pressed { get => _pressed; set => Set(ref _pressed, value); }
}

public sealed class SimLed : ObservableObject
{
    bool _lit, _present = true;
    public int Index { get; init; }
    public string Title => $"LED {Index}";
    public bool Lit { get => _lit; set => Set(ref _lit, value); }
    public bool Present { get => _present; set => Set(ref _present, value); }
}

public sealed class LogBuf
{
    readonly StringBuilder _sb = new();
    public string Text => _sb.ToString();
    public void Add(string s)
    {
        _sb.AppendLine($"{DateTime.Now:HH:mm:ss.fff}  {s}");
        if (_sb.Length > 30000) _sb.Remove(0, 10000);
    }
    public void Clear() => _sb.Clear();
}

/// <summary>Một dòng theo dõi bộ nhớ PLC ảo; sửa Value là ghi vào PlcMemory.</summary>
/// <summary>Một lần bàn phím ghi vào PLC ảo (hiện ở bảng 'Ghi từ bàn phím').</summary>
public sealed record PlcWriteRow(string Time, string Area, int Address, string Value, string Detail);

public sealed class WatchItem : ObservableObject
{
    readonly MainViewModel _vm;
    string _address = ""; string _value = "0"; bool _is32; string _info = "";
    public WatchItem(MainViewModel vm) => _vm = vm;

    public string Address { get => _address; set { if (Set(ref _address, value.Trim())) { Refresh(); RaiseRaw(nameof(AbsoluteAddress)); } } }
    /// <summary>Địa chỉ Modbus tuyệt đối (HR:100…) của cùng dòng; sửa ô nào thì ô kia đổi theo.</summary>
    public string AbsoluteAddress
    {
        get => AddressContext.ToAbsolute(_address);
        set { var s = AddressContext.FromAbsolute(value); if (s != _address) Address = s; else RaiseRaw(nameof(AbsoluteAddress)); }
    }
    public void RefreshAbsolute() => RaiseRaw(nameof(AbsoluteAddress));
    public bool Is32 { get => _is32; set { if (Set(ref _is32, value)) Refresh(); } }
    public string Info { get => _info; private set => Set(ref _info, value); }
    public string Value
    {
        get => _value;
        set
        {
            if (value == _value) return;
            if (long.TryParse(value, out var v)) { var err = _vm.WatchWrite(this, v); Info = err ?? Info; }
            Refresh();
        }
    }

    public void Refresh()
    {
        var (ok, v, info) = _vm.WatchRead(this);
        Info = info;
        var s = ok ? v.ToString() : "?";
        if (s != _value) { _value = s; RaiseRaw(nameof(Value)); }
    }
}

public sealed partial class MainViewModel
{
    public const int TabSim = 5, TabHw = 6;
    static readonly string[] KeyGridOrder = { "7", "8", "9", "ESC", "4", "5", "6", "UP", "1", "2", "3", "DN", "F1", "0", "F2", "ENT" };
    static readonly Dictionary<string, string> KeyTitles = new() { ["UP"] = "▲", ["DN"] = "▼" };

    // ---------- Mô phỏng ngoại tuyến ----------
    public PlcMemory Mem { get; } = new();
    public ObservableCollection<WatchItem> Watch { get; } = new();
    public ObservableCollection<SimKey> SimKeys { get; } = new();
    public ObservableCollection<SimLed> SimLeds { get; } = new();
    public SimKey SimK3 { get; } = new() { Id = "K3", Title = "K3", Configurable = true };
    readonly LogBuf _simLog = new();
    public string SimLogText => _simLog.Text;
    KeyboardEmulator? _emu;
    DispatcherTimer? _simTimer;
    byte[] _simUpper = new byte[4], _simLower = new byte[4];
    string _simStatus = "";
    bool _simLowerVisible = true;

    public byte[] SimUpper { get => _simUpper; private set => Set(ref _simUpper, value); }
    public byte[] SimLower { get => _simLower; private set => Set(ref _simLower, value); }
    public bool SimLowerVisible { get => _simLowerVisible; private set => Set(ref _simLowerVisible, value); }
    public int SimUpperDigits => SimUpper.Length;
    public string SimStatus { get => _simStatus; private set => Set(ref _simStatus, value); }
    public bool SimHasUpper => Project.Layout.HasUpper;
    public bool SimHasLower => Project.Layout.HasLower;

    public ICommand SimRestartCmd { get; private set; } = null!;
    public ICommand SimAddWatchCmd { get; private set; } = null!;
    public ICommand SimWatchFromProjectCmd { get; private set; } = null!;
    public ICommand SimClearMemCmd { get; private set; } = null!;
    public ICommand SimClearLogCmd { get; private set; } = null!;
    public ICommand SimRemoveWatchCmd { get; private set; } = null!;

    void InitSim()
    {
        _hwEngine = new ModbusSlaveEngine(_hwMem) { Station = 1 };
        _plcEngine = new ModbusSlaveEngine(Mem) { Station = 1 };
        SimRestartCmd = new RelayCommand(SimRestart);
        SimAddWatchCmd = new RelayCommand(() => Watch.Add(new WatchItem(this)));
        SimRemoveWatchCmd = new RelayCommand(o => { if (o is WatchItem w) Watch.Remove(w); });
        SimWatchFromProjectCmd = new RelayCommand(WatchFromProject);
        SimClearMemCmd = new RelayCommand(() => { Mem.Clear(); foreach (var w in Watch) w.Refresh(); });
        SimClearLogCmd = new RelayCommand(() => { _simLog.Clear(); Raise(nameof(SimLogText)); });
        _simTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
        _simTimer.Tick += (_, _) =>
        {
            if (_emu == null) return;
            _emu.Tick(100);
            foreach (var w in Watch) w.Refresh();
        };
        Mem.Written += (t, a, n) =>
        {
            // Đọc giá trị ngay tại luồng ghi: khi lên UI thì có thể đã bị ghi đè tiếp.
            var rows = new List<PlcWriteRow>();
            var now = DateTime.Now.ToString("HH:mm:ss.fff");
            bool bit = t.IsBitTable();
            for (int i = 0; i < Math.Min(n, 8); i++)
            {
                int ad = a + i;
                if (bit) { bool v = Mem.GetBit(t, ad); rows.Add(new PlcWriteRow(now, t.Display(), ad, v ? "1 (ON)" : "0 (OFF)", "")); }
                else
                {
                    ushort v = Mem.GetWord(t, ad);
                    rows.Add(new PlcWriteRow(now, t.Display(), ad, $"{v} (0x{v:X4})", $"{(short)v} có dấu · bit: {Convert.ToString(v, 2).PadLeft(16, '0')}"));
                }
            }
            Application.Current?.Dispatcher.BeginInvoke(() =>
            {
                foreach (var r in rows) { PlcWrites.Insert(0, r); _plcLog.Add($"PLC ← ghi {r.Area} {r.Address} = {r.Value}"); }
                if (n > rows.Count) _plcLog.Add($"PLC ← ghi {t.Display()} {a + rows.Count}…{a + n - 1} (còn {n - rows.Count} phần tử)");
                while (PlcWrites.Count > 300) PlcWrites.RemoveAt(PlcWrites.Count - 1);
                foreach (var w in Watch) w.Refresh();
                Raise(nameof(PlcLogText));
            });
        };
        InitHw();
    }

    void OnTabChanged()
    {
        if (SelectedTab == TabSim) { if (_emu == null) SimRestart(); _simTimer?.Start(); }
        else _simTimer?.Stop();
    }

    void SimProjectChanged()
    {
        Raise(nameof(SimHasUpper)); Raise(nameof(SimHasLower));
        if (_emu != null && SelectedTab == TabSim) SimRestart();
        else _emu = null;
    }

    void SimRestart()
    {
        _emu = new KeyboardEmulator(Project, CurrentProfile, Mem);
        _emu.Changed += SimUpdateView;
        _emu.Event += s => { _simLog.Add(s); Raise(nameof(SimLogText)); };

        SimKeys.Clear();
        foreach (var id in KeyGridOrder)
        {
            var cfg = Project.Keys.FirstOrDefault(k => k.Id == id);
            SimKeys.Add(new SimKey
            {
                Id = id, Title = KeyTitles.GetValueOrDefault(id, id), Configurable = cfg != null,
                Present = cfg?.Present ?? Project.Layout.IsPresent(id),
            });
        }
        SimK3.Present = Project.Keys.First(k => k.Id == "K3").Present;
        SimLeds.Clear();
        foreach (var l in Project.Leds) SimLeds.Add(new SimLed { Index = l.Index, Present = l.Present });
        _simLog.Add("Khởi động mô phỏng theo thiết kế hiện tại");
        Raise(nameof(SimLogText));
        SimUpdateView();
    }

    void SimUpdateView()
    {
        if (_emu == null) return;
        bool span = _emu.CurrentPage?.Upper.Span8 == true;
        int n = span ? 8 : 4;
        var seg = _emu.Segments;
        SimUpper = seg.Take(n).ToArray();
        SimLower = seg.Skip(4).Take(4).ToArray();
        SimLowerVisible = !span;
        Raise(nameof(SimUpperDigits));
        SimStatus = _emu.Status + (_emu.LastWrite.Length > 0 ? $"   ·   ghi gần nhất: {_emu.LastWrite}" : "");
        for (int i = 0; i < SimLeds.Count && i < 8; i++) SimLeds[i].Lit = _emu.LedLit[SimLeds[i].Index - 1];
    }

    public void SimPress(string id, bool down)
    {
        if (_emu == null) return;
        var k = SimKeys.FirstOrDefault(x => x.Id == id) ?? (id == "K3" ? SimK3 : null);
        if (k != null) k.Pressed = down;
        if (down) _emu.Press(id); else _emu.Release(id);
        foreach (var w in Watch) w.Refresh();
    }

    // ---------- Theo dõi bộ nhớ ----------
    public (bool ok, long value, string info) WatchRead(WatchItem w)
    {
        if (w.Address.Length == 0) return (false, 0, "");
        var r = AddressResolver.Resolve(w.Address, CurrentProfile, Project.Comm);
        if (!r.Ok) return (false, 0, r.Error);
        var m = r.Ref;
        if (m.Table.IsBitTable()) return (true, Mem.GetBit(m.Table, m.Address) ? 1 : 0, m.ToString());
        ushort w0 = Mem.GetWord(m.Table, m.Address);
        if (m.HasBit) return (true, (w0 >> m.Bit) & 1, m.ToString());
        if (!w.Is32) return (true, (short)w0, m.ToString());
        ushort w1 = Mem.GetWord(m.Table, m.Address + 1);
        uint u = Project.Comm.WordOrder == WordOrder.CDAB ? ((uint)w1 << 16) | w0 : ((uint)w0 << 16) | w1;
        return (true, (int)u, m.ToString() + " (32 bit)");
    }

    public string? WatchWrite(WatchItem w, long v)
    {
        var r = AddressResolver.Resolve(w.Address, CurrentProfile, Project.Comm);
        if (!r.Ok) return r.Error;
        var m = r.Ref;
        if (m.Table.IsBitTable()) { Mem.SetBit(m.Table, m.Address, v != 0); return null; }
        if (m.HasBit)
        {
            ushort x = Mem.GetWord(m.Table, m.Address);
            Mem.SetWord(m.Table, m.Address, v != 0 ? (ushort)(x | (1 << m.Bit)) : (ushort)(x & ~(1 << m.Bit)));
            return null;
        }
        if (!w.Is32) { Mem.SetWord(m.Table, m.Address, (ushort)(short)v); return null; }
        uint u = (uint)v; ushort hi = (ushort)(u >> 16), lo = (ushort)u;
        bool low1 = Project.Comm.WordOrder == WordOrder.CDAB;
        Mem.SetWord(m.Table, m.Address, low1 ? lo : hi);
        Mem.SetWord(m.Table, m.Address + 1, low1 ? hi : lo);
        return null;
    }

    void WatchFromProject()
    {
        Recompute();
        foreach (var row in AddressMap)
        {
            if (Watch.Any(x => x.Address.Equals(row.Plc, StringComparison.OrdinalIgnoreCase))) continue;
            Watch.Add(new WatchItem(this) { Address = row.Plc, Is32 = row.Type is "Int32" or "UInt32" });
        }
    }

    // ---------- Thử phần cứng thật (PC làm master, firmware hiện tại) ----------
    public ObservableCollection<PortChoice> HwPorts { get; } = new();
    public ObservableCollection<SimKey> HwKeys { get; } = new();
    public ObservableCollection<SimLed> HwLeds { get; } = new();
    readonly LogBuf _hwLog = new();
    readonly object _hwLock = new();
    readonly PlcMemory _hwMem = new();
    ModbusSlaveEngine _hwEngine = null!;
    CancellationTokenSource? _hwCts;
    HardwareProbe? _probe;
    PortChoice? _hwPort;
    int _hwStation = 1, _hwBaud = 9600, _hwCounter, _hwRow1 = 1234, _hwRow2 = 5678;
    bool _hwRunning; string _hwStatus = "Chưa kết nối";

    public string HwLogText => _hwLog.Text;
    public PortChoice? HwPort { get => _hwPort; set => Set(ref _hwPort, value); }
    public int HwStation { get => _hwStation; set => Set(ref _hwStation, value); }
    public int HwBaud { get => _hwBaud; set => Set(ref _hwBaud, value); }
    public int HwCounter { get => _hwCounter; private set => Set(ref _hwCounter, value); }
    public int HwRow1 { get => _hwRow1; set => Set(ref _hwRow1, value); }
    public int HwRow2 { get => _hwRow2; set => Set(ref _hwRow2, value); }
    public bool HwRunning { get => _hwRunning; private set { if (Set(ref _hwRunning, value)) { RaiseRaw(nameof(HwButtonText)); CommandManager.InvalidateRequerySuggested(); } } }
    public string HwButtonText => HwRunning ? "Dừng" : "Bắt đầu thử";
    public string HwStatus { get => _hwStatus; private set => Set(ref _hwStatus, value); }
    bool HwVirtual => HwPort?.IsSimulated == true;

    public ICommand HwToggleCmd { get; private set; } = null!;
    public ICommand HwWriteDisplayCmd { get; private set; } = null!;
    public ICommand HwLedCmd { get; private set; } = null!;
    public ICommand HwAutoTestCmd { get; private set; } = null!;
    public ICommand HwClearLogCmd { get; private set; } = null!;

    static readonly PortChoice VirtualFw = new("Firmware ảo (PC giả lập bàn phím)", true);

    void InitHw()
    {
        foreach (var id in KeyGridOrder)
            HwKeys.Add(new SimKey { Id = id, Title = KeyTitles.GetValueOrDefault(id, id) });
        for (int i = 1; i <= 8; i++) HwLeds.Add(new SimLed { Index = i });
        RefreshHwPorts();
        HwToggleCmd = new RelayCommand(HwToggle, () => HwPort != null);
        HwWriteDisplayCmd = new RelayCommand(() => HwDo("Ghi hàng số", p => p.WriteDisplay((ushort)Math.Clamp(HwRow1, 0, 9999), (ushort)Math.Clamp(HwRow2, 0, 9999))), () => HwRunning);
        HwLedCmd = new RelayCommand(HwWriteLeds, () => HwRunning);
        HwAutoTestCmd = new RelayCommand(HwAutoTest, () => HwRunning);
        HwClearLogCmd = new RelayCommand(() => { _hwLog.Clear(); Raise(nameof(HwLogText)); });
        InitPlcServer();
    }

    void RefreshHwPorts()
    {
        HwPorts.Clear();
        foreach (var n in SerialPort.GetPortNames().OrderBy(x => x.Length).ThenBy(x => x)) HwPorts.Add(new PortChoice(n, false));
        HwPorts.Add(VirtualFw);
        HwPort ??= HwPorts.First();
        PlcPorts.Clear();
        foreach (var p in HwPorts.Where(x => !x.IsSimulated)) PlcPorts.Add(p);
        PlcPort ??= PlcPorts.FirstOrDefault();
    }

    void HwLog(string s) { _hwLog.Add(s); Raise(nameof(HwLogText)); }

    void HwToggle()
    {
        if (HwRunning) { HwStop(); return; }
        var choice = HwPort; if (choice == null) return;
        IModbusPort port;
        if (choice.IsSimulated)
        {
            _hwEngine.Station = (byte)Math.Clamp(HwStation, 1, 247);
            port = new EnginePort(_hwEngine, choice.Name);
        }
        else port = new SerialModbusPort(choice.Name, HwBaud);
        try { port.Open(); }
        catch (Exception ex) { HwLog("✖ Không mở được cổng: " + ex.Message); port.Dispose(); return; }

        _probe = new HardwareProbe(port, (byte)Math.Clamp(HwStation, 1, 247));
        _probe.Frame += (tx, f) => Application.Current.Dispatcher.BeginInvoke(() => HwLog((tx ? "TX " : "RX ") + ModbusRtu.Hex(f)));
        var cts = _hwCts = new CancellationTokenSource();
        HwRunning = true; HwStatus = "Đang thử…"; HwLog($"▶ Bắt đầu: {choice.Name}, trạm {HwStation}" + (choice.IsSimulated ? "" : $", {HwBaud} 8N1"));
        var probe = _probe;
        Task.Run(async () =>
        {
            ushort lastKeys = 0; int lastCount = -1;
            try
            {
                while (!cts.IsCancellationRequested)
                {
                    (ushort Keys, ushort Counter, ushort Row1, ushort Row2, ushort Leds) r;
                    lock (_hwLock) r = probe.Read();
                    var keys = r.Keys; var cnt = r.Counter; var old = lastKeys; var oldCnt = lastCount;
                    await Application.Current.Dispatcher.InvokeAsync(() =>
                    {
                        HwStatus = "Đã kết nối — bấm phím trên bàn phím thật";
                        HwCounter = cnt;
                        for (int i = 0; i < 16; i++)
                        {
                            bool down = (keys & (1 << i)) != 0;
                            var k = HwKeys.First(x => x.Id == HardwareProbe.KeyByBit[i]);
                            if (down && (old & (1 << i)) == 0) HwLog($"Phím {k.Title} nhấn (bộ đếm {cnt})");
                            k.Pressed = down;
                        }
                        if (oldCnt >= 0 && cnt != oldCnt && keys == 0) HwLog($"Bộ đếm tăng {oldCnt} → {cnt} (phím nhả nhanh)");
                    });
                    lastKeys = keys; lastCount = cnt;
                    await Task.Delay(60, cts.Token);
                }
            }
            catch (OperationCanceledException) { }
            catch (Exception ex)
            {
                await Application.Current.Dispatcher.InvokeAsync(() => { HwLog("✖ " + ex.Message); HwStatus = "Mất kết nối"; });
            }
            finally
            {
                port.Dispose();
                await Application.Current.Dispatcher.InvokeAsync(() => { HwRunning = false; foreach (var k in HwKeys) k.Pressed = false; });
            }
        });
    }

    void HwStop() { _hwCts?.Cancel(); HwStatus = "Đã dừng"; HwLog("■ Dừng"); }

    void HwDo(string what, Action<HardwareProbe> act)
    {
        var p = _probe; if (p == null) return;
        Task.Run(() =>
        {
            try { lock (_hwLock) act(p); }
            catch (Exception ex) { Application.Current.Dispatcher.BeginInvoke(() => HwLog($"✖ {what}: {ex.Message}")); }
        });
    }

    void HwWriteLeds()
    {
        ushort mask = 0;
        foreach (var l in HwLeds) if (l.Lit) mask |= (ushort)(1 << (l.Index - 1));
        HwDo("Ghi LED", p => p.Write(HardwareProbe.RegLeds, mask));
    }

    void HwAutoTest()
    {
        var p = _probe; if (p == null) return;
        HwLog("▶ Chạy thử tự động: LED chạy dần, rồi đếm hàng số");
        Task.Run(() =>
        {
            try
            {
                for (int i = 0; i < 8; i++) { lock (_hwLock) p.Write(HardwareProbe.RegLeds, (ushort)(1 << i)); Thread.Sleep(150); }
                lock (_hwLock) p.Write(HardwareProbe.RegLeds, 0xFF);
                for (ushort v = 1111; v <= 9999; v += 1111) { lock (_hwLock) p.WriteDisplay(v, (ushort)(10000 - v)); Thread.Sleep(200); }
                lock (_hwLock) { p.Write(HardwareProbe.RegLeds, 0); p.WriteDisplay(0, 0); }
                Application.Current.Dispatcher.BeginInvoke(() => HwLog("✔ Xong. Nếu thấy thiếu đoạn/LED/số sai thứ tự thì kiểm tra hàn và ma trận."));
            }
            catch (Exception ex) { Application.Current.Dispatcher.BeginInvoke(() => HwLog("✖ " + ex.Message)); }
        });
    }

    /// <summary>Firmware ảo: bấm phím trên máy tính khi chọn "Firmware ảo" làm bàn phím thật giả lập.</summary>
    public void HwVirtualPress(string id, bool down)
    {
        if (!HwRunning || !HwVirtual) return;
        int bit = Array.IndexOf(HardwareProbe.KeyByBit, id);
        if (bit < 0) return;
        ushort keys = _hwMem.GetWord(ModbusTable.Holding, HardwareProbe.RegKeys);
        ushort nk = down ? (ushort)(keys | (1 << bit)) : (ushort)(keys & ~(1 << bit));
        _hwMem.SetWord(ModbusTable.Holding, HardwareProbe.RegKeys, nk);
        if (down) _hwMem.SetWord(ModbusTable.Holding, HardwareProbe.RegCounter, (ushort)(_hwMem.GetWord(ModbusTable.Holding, HardwareProbe.RegCounter) + 1));
    }

    // ---------- PLC ảo trên cổng COM ----------
    public ObservableCollection<PortChoice> PlcPorts { get; } = new();
    readonly LogBuf _plcLog = new();
    ModbusSlaveEngine _plcEngine = null!;
    SerialPlcServer? _plcServer;
    PortChoice? _plcPort; int _plcBaud = 9600, _plcStation = 1; ParityMode _plcParity; bool _plcRunning; int _plcFrames; string _plcStatus = "Chưa chạy";

    public string PlcLogText => _plcLog.Text;
    public PortChoice? PlcPort { get => _plcPort; set => Set(ref _plcPort, value); }
    public int PlcBaud { get => _plcBaud; set => Set(ref _plcBaud, value); }
    public int PlcStation { get => _plcStation; set => Set(ref _plcStation, value); }
    public ParityMode PlcParity { get => _plcParity; set => Set(ref _plcParity, value); }
    public bool PlcRunning { get => _plcRunning; private set { if (Set(ref _plcRunning, value)) RaiseRaw(nameof(PlcButtonText)); } }
    public string PlcButtonText => PlcRunning ? "Dừng PLC ảo" : "Chạy PLC ảo";
    public int PlcFrames { get => _plcFrames; private set => Set(ref _plcFrames, value); }
    public string PlcStatus { get => _plcStatus; private set => Set(ref _plcStatus, value); }
    public ICommand PlcToggleCmd { get; private set; } = null!;
    public ICommand PlcClearLogCmd { get; private set; } = null!;
    public ObservableCollection<PlcWriteRow> PlcWrites { get; } = new();

    // Mô phỏng PLC lỗi để thử cách bàn phím phản ứng (E-CO, E-Ad, E-Fn, E-dA…).
    public bool PlcSilent { get => _plcEngine.Silent; set { _plcEngine.Silent = value; RaiseRaw(nameof(PlcSilent)); } }
    public int PlcDelayMs { get => _plcEngine.ResponseDelayMs; set { _plcEngine.ResponseDelayMs = Math.Clamp(value, 0, 2000); RaiseRaw(nameof(PlcDelayMs)); } }
    public int PlcForceException { get => _plcEngine.ForceException; set { _plcEngine.ForceException = (byte)Math.Clamp(value, 0, 4); RaiseRaw(nameof(PlcForceException)); } }

    void InitPlcServer()
    {
        PlcToggleCmd = new RelayCommand(PlcToggle, () => PlcPort != null || PlcRunning);
        PlcClearLogCmd = new RelayCommand(() => { _plcLog.Clear(); PlcWrites.Clear(); Raise(nameof(PlcLogText)); });
    }

    void PlcToggle()
    {
        if (PlcRunning)
        {
            _plcServer?.Stop(); _plcServer = null; PlcRunning = false; PlcStatus = "Đã dừng";
            _plcLog.Add("■ PLC ảo dừng"); Raise(nameof(PlcLogText)); return;
        }
        if (PlcPort == null) return;
        _plcEngine.Station = (byte)Math.Clamp(PlcStation, 1, 247);
        var srv = new SerialPlcServer(_plcEngine);
        srv.Frame += (rx, f) => Application.Current.Dispatcher.BeginInvoke(() =>
        {
            _plcLog.Add((rx ? "RX " : "TX ") + ModbusRtu.Hex(f)); PlcFrames = srv.FramesOk; Raise(nameof(PlcLogText));
        });
        srv.Error += e => Application.Current.Dispatcher.BeginInvoke(() =>
        {
            _plcLog.Add("✖ " + e); PlcRunning = false; PlcStatus = "Lỗi cổng"; Raise(nameof(PlcLogText));
        });
        try
        {
            srv.Start(PlcPort.Name, PlcBaud, PlcParity switch { ParityMode.Even => Parity.Even, ParityMode.Odd => Parity.Odd, _ => Parity.None }, StopBits.One);
        }
        catch (Exception ex) { _plcLog.Add("✖ Không mở được cổng: " + ex.Message); Raise(nameof(PlcLogText)); return; }
        _plcServer = srv; PlcRunning = true; PlcFrames = 0;
        PlcStatus = $"Đang chạy trên {PlcPort.Name}, trạm {PlcStation}";
        _plcLog.Add($"▶ PLC ảo: {PlcPort.Name} {PlcBaud} trạm {PlcStation}"); Raise(nameof(PlcLogText));
    }

    public void ShutdownSim()
    {
        _hwCts?.Cancel();
        _plcServer?.Stop();
    }
}
