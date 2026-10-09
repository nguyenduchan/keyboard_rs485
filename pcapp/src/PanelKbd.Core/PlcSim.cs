using System.IO.Ports;

namespace PanelKbd.Core;

/// <summary>Bộ nhớ của PLC ảo: 4 bảng Modbus, mỗi bảng 65536 phần tử.</summary>
public sealed class PlcMemory
{
    readonly ushort[] _holding = new ushort[65536];
    readonly ushort[] _input = new ushort[65536];
    readonly bool[] _coil = new bool[65536];
    readonly bool[] _discrete = new bool[65536];
    readonly object _lock = new();

    /// <summary>Gọi sau khi có ghi từ bên ngoài (khung Modbus): bảng, địa chỉ đầu, số phần tử.</summary>
    public event Action<ModbusTable, int, int>? Written;

    public ushort GetWord(ModbusTable t, int a) { lock (_lock) return (t == ModbusTable.Input ? _input : _holding)[a & 0xFFFF]; }
    public void SetWord(ModbusTable t, int a, ushort v, bool notify = false)
    {
        lock (_lock) (t == ModbusTable.Input ? _input : _holding)[a & 0xFFFF] = v;
        if (notify) Written?.Invoke(t, a, 1);
    }
    public bool GetBit(ModbusTable t, int a) { lock (_lock) return (t == ModbusTable.Discrete ? _discrete : _coil)[a & 0xFFFF]; }
    public void SetBit(ModbusTable t, int a, bool v, bool notify = false)
    {
        lock (_lock) (t == ModbusTable.Discrete ? _discrete : _coil)[a & 0xFFFF] = v;
        if (notify) Written?.Invoke(t, a, 1);
    }
    internal void Notify(ModbusTable t, int a, int n) => Written?.Invoke(t, a, n);

    public void Clear()
    {
        lock (_lock) { Array.Clear(_holding); Array.Clear(_input); Array.Clear(_coil); Array.Clear(_discrete); }
    }
}

/// <summary>Modbus RTU slave dùng PlcMemory: lệnh 01, 02, 03, 04, 05, 06, 15, 16.</summary>
public sealed class ModbusSlaveEngine
{
    readonly PlcMemory _mem;
    public byte Station { get; set; } = 1;
    /// <summary>Mô phỏng PLC lỗi: 0 = bình thường; khác 0 = trả exception với mã này cho mọi yêu cầu.</summary>
    public byte ForceException { get; set; }
    /// <summary>Mô phỏng PLC không trả lời (bàn phím phải báo E-CO).</summary>
    public bool Silent { get; set; }
    /// <summary>Thời gian PLC "xử lý" trước khi trả lời (ms); PLC thật thường 5–50 ms.</summary>
    public int ResponseDelayMs { get; set; }
    public ModbusSlaveEngine(PlcMemory mem) => _mem = mem;

    /// <summary>Độ dài khung yêu cầu đầy đủ nếu đã biết từ phần đầu; -1 nếu chưa đủ byte để biết.</summary>
    public static int ExpectedRequestLength(ReadOnlySpan<byte> b)
    {
        if (b.Length < 2) return -1;
        return b[1] switch
        {
            1 or 2 or 3 or 4 or 5 or 6 => 8,
            15 or 16 => b.Length >= 7 ? 9 + b[6] : -1,
            _ => 8,
        };
    }

    static byte[] Exc(byte st, byte fc, byte code) => ModbusRtu.WithCrc(new[] { st, (byte)(fc | 0x80), code });

    /// <summary>Trả về khung trả lời, hoặc null nếu không phải trạm này / khung hỏng / broadcast.</summary>
    public byte[]? Handle(byte[] req)
    {
        if (req.Length < 8 || !ModbusRtu.CrcOk(req)) return null;
        byte st = req[0], fc = req[1];
        bool broadcast = st == 0;
        if (!broadcast && st != Station) return null;
        if (Silent) return null;
        if (ForceException != 0) return broadcast ? null : Exc(st, fc, ForceException);
        if (fc is 1 or 2 or 3 or 4 or 5 or 6 && req.Length != 8) return null;   // khung sai độ dài: bỏ qua như PLC thật
        int addr = (req[2] << 8) | req[3];
        int qty = (req[4] << 8) | req[5];
        byte[]? rsp;
        switch (fc)
        {
            case 1:
            case 2:
            {
                if (qty < 1 || qty > 2000) { rsp = Exc(st, fc, 3); break; }
                if (addr + qty > 65536) { rsp = Exc(st, fc, 2); break; }
                var t = fc == 1 ? ModbusTable.Coil : ModbusTable.Discrete;
                var pdu = new byte[3 + (qty + 7) / 8];
                pdu[0] = st; pdu[1] = fc; pdu[2] = (byte)((qty + 7) / 8);
                for (int i = 0; i < qty; i++) if (_mem.GetBit(t, addr + i)) pdu[3 + i / 8] |= (byte)(1 << (i % 8));
                rsp = ModbusRtu.WithCrc(pdu); break;
            }
            case 3:
            case 4:
            {
                if (qty < 1 || qty > 125) { rsp = Exc(st, fc, 3); break; }
                if (addr + qty > 65536) { rsp = Exc(st, fc, 2); break; }
                var t = fc == 3 ? ModbusTable.Holding : ModbusTable.Input;
                var pdu = new byte[3 + 2 * qty];
                pdu[0] = st; pdu[1] = fc; pdu[2] = (byte)(2 * qty);
                for (int i = 0; i < qty; i++) { var v = _mem.GetWord(t, addr + i); pdu[3 + 2 * i] = (byte)(v >> 8); pdu[4 + 2 * i] = (byte)v; }
                rsp = ModbusRtu.WithCrc(pdu); break;
            }
            case 5:
            {
                int v = (req[4] << 8) | req[5];
                if (v != 0x0000 && v != 0xFF00) { rsp = Exc(st, fc, 3); break; }
                _mem.SetBit(ModbusTable.Coil, addr, v == 0xFF00); _mem.Notify(ModbusTable.Coil, addr, 1);
                rsp = req; break;
            }
            case 6:
                _mem.SetWord(ModbusTable.Holding, addr, (ushort)qty); _mem.Notify(ModbusTable.Holding, addr, 1);
                rsp = req; break;
            case 15:
            {
                if (qty < 1 || qty > 1968 || req.Length < 9 || req[6] != (qty + 7) / 8 || req.Length != 9 + req[6]) { rsp = Exc(st, fc, 3); break; }
                if (addr + qty > 65536) { rsp = Exc(st, fc, 2); break; }
                for (int i = 0; i < qty; i++) _mem.SetBit(ModbusTable.Coil, addr + i, (req[7 + i / 8] & (1 << (i % 8))) != 0);
                _mem.Notify(ModbusTable.Coil, addr, qty);
                rsp = ModbusRtu.WithCrc(req.AsSpan(0, 6).ToArray()); break;
            }
            case 16:
            {
                if (qty < 1 || qty > 123 || req[6] != 2 * qty || req.Length != 9 + 2 * qty) { rsp = Exc(st, fc, 3); break; }
                if (addr + qty > 65536) { rsp = Exc(st, fc, 2); break; }
                for (int i = 0; i < qty; i++) _mem.SetWord(ModbusTable.Holding, addr + i, (ushort)((req[7 + 2 * i] << 8) | req[8 + 2 * i]));
                _mem.Notify(ModbusTable.Holding, addr, qty);
                rsp = ModbusRtu.WithCrc(req.AsSpan(0, 6).ToArray()); break;
            }
            default: rsp = Exc(st, fc, 1); break;
        }
        return broadcast ? null : rsp;
    }
}

/// <summary>
/// PLC ảo trên cổng COM thật: cắm bàn phím (chế độ master) vào bộ chuyển USB–RS485 của PC,
/// PC trả lời như một PLC Modbus slave. Không cần PLC thật.
/// </summary>
public sealed class SerialPlcServer : IDisposable
{
    readonly ModbusSlaveEngine _engine;
    SerialPort? _sp;
    Thread? _thread;
    volatile bool _run;

    public event Action<bool, byte[]>? Frame;   // true = nhận từ bàn phím, false = PC trả lời
    public event Action<string>? Error;
    public bool IsRunning => _run;
    public int FramesOk { get; private set; }

    public SerialPlcServer(ModbusSlaveEngine engine) => _engine = engine;

    public void Start(string port, int baud, Parity parity, StopBits stop)
    {
        Stop();
        _sp = new SerialPort(port, baud, parity, 8, stop) { ReadTimeout = 20, WriteTimeout = 500 };
        _sp.Open();
        _run = true;
        _thread = new Thread(Loop) { IsBackground = true, Name = "PlcSim" };
        _thread.Start();
    }

    static long Ms() => System.Diagnostics.Stopwatch.GetTimestamp() * 1000 / System.Diagnostics.Stopwatch.Frequency;

    void Loop()
    {
        var buf = new List<byte>();
        long last = 0;
        try
        {
            while (_run)
            {
                int n = _sp!.BytesToRead;
                if (n > 0)
                {
                    var tmp = new byte[n];
                    int got = _sp.Read(tmp, 0, n);
                    if (buf.Count > 0 && Ms() - last > 50) buf.Clear();       // rác cũ còn sót
                    buf.AddRange(tmp.Take(got));
                    last = Ms();
                }
                if (buf.Count > 0)
                {
                    // Khung đủ độ dài thì xử lý ngay; khung không rõ độ dài thì chờ im lặng. Bộ USB–RS485 có thể
                    // chia khung thành nhiều gói cách nhau hàng chục ms nên không dựa vào t3.5 ngắn.
                    int exp = ModbusSlaveEngine.ExpectedRequestLength(buf.ToArray());
                    bool complete = exp > 0 && buf.Count >= exp;
                    bool idle = Ms() - last >= (exp > 0 ? 100 : 25);
                    if (complete || idle)
                    {
                        var req = (exp > 0 && buf.Count >= exp ? buf.Take(exp) : buf).ToArray();
                        buf.RemoveRange(0, Math.Min(buf.Count, req.Length));
                        Frame?.Invoke(true, req);
                        var rsp = _engine.Handle(req);
                        if (rsp != null)
                        {
                            if (_engine.ResponseDelayMs > 0) Thread.Sleep(Math.Min(_engine.ResponseDelayMs, 2000));
                            _sp.Write(rsp, 0, rsp.Length); FramesOk++; Frame?.Invoke(false, rsp);
                        }
                        continue;
                    }
                }
                Thread.Sleep(1);
            }
        }
        catch (Exception ex) { if (_run) Error?.Invoke(ex.Message); }
        _run = false;
    }

    public void Stop()
    {
        _run = false;
        _thread?.Join(300); _thread = null;
        try { _sp?.Close(); _sp?.Dispose(); } catch { }
        _sp = null;
    }

    public void Dispose() => Stop();
}

/// <summary>Cổng ảo nối thẳng vào một ModbusSlaveEngine (dùng cho thử nghiệm không cần phần cứng).</summary>
public sealed class EnginePort : IModbusPort
{
    readonly ModbusSlaveEngine _e;
    public string Name { get; }
    public EnginePort(ModbusSlaveEngine e, string name = "PLC ảo") { _e = e; Name = name; }
    public void Open() { }
    public void Close() { }
    public void Dispose() { }
    public byte[]? Exchange(byte[] request, int timeoutMs) => _e.Handle(request);
}
