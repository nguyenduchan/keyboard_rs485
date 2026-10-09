using System.Buffers.Binary;
using System.IO.Ports;

namespace PanelKbd.Core;

public static class ModbusRtu
{
    public static ushort Crc16(ReadOnlySpan<byte> d)
    {
        ushort crc = 0xFFFF;
        foreach (var b in d)
        {
            crc ^= b;
            for (int i = 0; i < 8; i++) crc = (crc & 1) != 0 ? (ushort)((crc >> 1) ^ 0xA001) : (ushort)(crc >> 1);
        }
        return crc;
    }

    public static byte[] WithCrc(byte[] pdu)
    {
        var f = new byte[pdu.Length + 2];
        pdu.CopyTo(f, 0);
        var c = Crc16(pdu);
        f[^2] = (byte)c; f[^1] = (byte)(c >> 8);
        return f;
    }

    public static bool CrcOk(byte[] f) => f.Length >= 4 && Crc16(f.AsSpan(0, f.Length - 2)) == (ushort)(f[^2] | (f[^1] << 8));

    public static byte[] ReadRegs(byte st, byte fc, ushort addr, ushort count)
        => WithCrc(new[] { st, fc, (byte)(addr >> 8), (byte)addr, (byte)(count >> 8), (byte)count });

    public static byte[] WriteReg(byte st, ushort addr, ushort val)
        => WithCrc(new byte[] { st, 6, (byte)(addr >> 8), (byte)addr, (byte)(val >> 8), (byte)val });

    public static byte[] WriteRegs(byte st, ushort addr, ReadOnlySpan<ushort> vals)
    {
        var pdu = new byte[7 + vals.Length * 2];
        pdu[0] = st; pdu[1] = 16; pdu[2] = (byte)(addr >> 8); pdu[3] = (byte)addr;
        pdu[4] = (byte)(vals.Length >> 8); pdu[5] = (byte)vals.Length; pdu[6] = (byte)(vals.Length * 2);
        for (int i = 0; i < vals.Length; i++) { pdu[7 + 2 * i] = (byte)(vals[i] >> 8); pdu[8 + 2 * i] = (byte)vals[i]; }
        return WithCrc(pdu);
    }

    public static string Hex(byte[] f) => string.Join(' ', f.Select(b => b.ToString("X2")));

    public static string ExceptionText(byte code) => code switch
    {
        1 => "Illegal Function", 2 => "Illegal Data Address", 3 => "Illegal Data Value",
        4 => "Device Failure", _ => $"mã {code}",
    };
}

public sealed class ConfigException : Exception
{
    public ConfigException(string m) : base(m) { }
}

public interface IModbusPort : IDisposable
{
    string Name { get; }
    void Open();
    void Close();
    /// <summary>Gửi khung, đợi trả lời; null nếu hết giờ.</summary>
    byte[]? Exchange(byte[] request, int timeoutMs);
}

public sealed class SerialModbusPort : IModbusPort
{
    readonly SerialPort _sp;
    public string Name { get; }

    public SerialModbusPort(string portName, int baud = 9600, Parity parity = Parity.None, StopBits stop = StopBits.One)
    {
        Name = portName;
        _sp = new SerialPort(portName, baud, parity, 8, stop) { ReadTimeout = 50, WriteTimeout = 500 };
    }

    // TickCount64 trên Windows chỉ chính xác ~15 ms: dùng Stopwatch cho các ngưỡng thời gian ngắn.
    static long Ms() => System.Diagnostics.Stopwatch.GetTimestamp() * 1000 / System.Diagnostics.Stopwatch.Frequency;

    public void Open() { if (!_sp.IsOpen) _sp.Open(); }
    public void Close() { if (_sp.IsOpen) _sp.Close(); }

    public byte[]? Exchange(byte[] request, int timeoutMs)
    {
        _sp.DiscardInBuffer();
        _sp.Write(request, 0, request.Length);
        var buf = new List<byte>();
        var start = Ms();
        long last = 0;
        while (true)
        {
            int avail = _sp.BytesToRead;
            if (avail > 0)
            {
                var tmp = new byte[avail];
                int n = _sp.Read(tmp, 0, avail);
                buf.AddRange(tmp.Take(n));
                last = Ms();
            }
            else
            {
                long now = Ms();
                if (buf.Count > 0 && now - last >= 15) break;   // khoảng lặng ≥ t3.5 (dư cho 9600 baud)
                if (buf.Count == 0 && now - start >= timeoutMs) return null;
                Thread.Sleep(1);
            }
        }
        return buf.ToArray();
    }

    public void Dispose() { try { _sp.Dispose(); } catch { } }
}

public sealed class DeviceInfo
{
    public ushort Model { get; init; }
    public byte FwMajor { get; init; }
    public byte FwMinor { get; init; }
    public ushort Patch { get; init; }
    public ushort StructVersion { get; init; }
    public uint Serial { get; init; }
    public ushort Caps { get; init; }
    public ushort MaxPages { get; init; }
    public ushort ImageBytes { get; init; }
    public ushort MaxImageBytes { get; init; }
    public ushort Status { get; init; }
    public ushort LastResult { get; init; }
    public ushort LastError { get; init; }
    public uint UptimeSec { get; init; }
    public ushort FlashSeq { get; init; }
    public ushort Revision { get; init; }

    public bool CanMaster => (Caps & 0x02) != 0;
    public bool CanSlave => (Caps & 0x04) != 0;
    public bool InConfigMode => (Caps & 0x10) != 0 || (Status & 0x08) != 0;
    /// <summary>Bit0 của trạng thái = cấu hình đã nạp được từ flash; 0 nghĩa là đang dùng mặc định (E-cF).</summary>
    public bool ConfigDefaulted => (Status & 1) == 0;
    public bool PlcLost => (Status & 2) != 0;
    public bool Unsaved => (Status & 4) != 0;
    public bool PowerOk => (Status & 0x10) != 0;
    public string Firmware => $"{FwMajor}.{FwMinor}.{Patch}";
    public override string ToString()
        => $"Model {Model}, FW {Firmware}, schema v{StructVersion}, serial {Serial:X8}, tối đa {MaxPages} trang, khối {ImageBytes} B" +
           (InConfigMode ? ", đang ở chế độ cấu hình" : ", đang chạy") +
           (ConfigDefaulted ? " — dùng cấu hình mặc định (E-cF)" : "") + (Unsaved ? " — có thay đổi chưa lưu" : "");
}

/// <summary>Bộ đếm chẩn đoán (thanh ghi 130–135).</summary>
public sealed class Diagnostics
{
    public ushort MasterOk { get; init; }
    public ushort Timeouts { get; init; }
    public ushort CrcErrors { get; init; }
    public ushort Exceptions { get; init; }
    public ushort LastErrorKind { get; init; }
    public byte LastExceptionCode { get; init; }
    public ushort SlaveServed { get; init; }
    public override string ToString()
        => $"master OK {MasterOk}, mất liên lạc {Timeouts}, lỗi CRC/khung {CrcErrors}, exception từ PLC {Exceptions} " +
           $"(gần nhất: loại {LastErrorKind}, mã {LastExceptionCode}), yêu cầu slave đã phục vụ {SlaveServed}";
}

public enum ConfigCommand : ushort { Apply = 1, Reload = 2, Factory = 3, Identify = 4, Exit = 5, EnterConfig = 6, ClearDiag = 7 }

/// <summary>Cửa sổ trạng thái thời gian thực của bàn phím (thanh ghi 0–6, 10–16) — để thử phần cứng khi đang ở chế độ cấu hình.</summary>
public sealed class LiveState
{
    public ushort KeyCode { get; init; }
    public ushort KeyCount { get; init; }
    public uint KeysDown { get; init; }          // bit n-1 = SWn (SW1..SW24)
    public ushort Flags { get; init; }
    public int Top { get; init; }
    public int Bottom { get; init; }
    public ushort Led { get; init; }
}

/// <summary>
/// Tập lệnh cấu hình qua Modbus RTU (docs/CONFIG_PROTOCOL.md): bắt tay, đọc thông tin, sao lưu/ghi khối cấu hình,
/// APPLY/RELOAD/FACTORY/IDENTIFY/EXIT/ENTER_CONFIG/CLEAR_DIAG, chẩn đoán, thử phím–màn–LED–còi.
/// Không nạp firmware.
/// </summary>
public sealed class ConfigClient
{
    public const byte DefaultStation = 247;
    public const ushort InfoBase = 100, InfoCount = 18;
    public const ushort RegCmd = 120, RegResult = 121;
    public const ushort DiagBase = 130, DiagCount = 6;
    public const ushort BufBase = 200;
    public const int BufRegs = 959;                 // 1918 byte
    public const int ChunkRegs = 60;
    public const ushort InfoMagic = 0x504B;

    // Thanh ghi slave (thử phần cứng)
    public const ushort RegKeyCode = 0, RegKeyCount = 1, RegFlags = 4, RegKeysLo = 5, RegKeysExt = 6,
        RegTopHi = 10, RegLed = 14, RegCtrl = 15, RegFmt = 16, RegRaw0 = 20;
    public const ushort CtrlBeepShort = 0x0100, CtrlBeepLong = 0x0200, CtrlKeyLock = 0x0400, FmtRaw = 0x1000;

    readonly IModbusPort _port;
    public byte Station { get; set; } = DefaultStation;
    public int TimeoutMs { get; set; } = 300;
    public int Retries { get; set; } = 2;
    /// <summary>Gọi cho mỗi khung: (true = gửi, khung).</summary>
    public event Action<bool, byte[]>? Frame;

    public ConfigClient(IModbusPort port) => _port = port;

    byte[] Transact(byte[] req, byte expectFc, int expectLen, int? retries = null)
    {
        int maxRetries = retries ?? Retries;
        for (int attempt = 0; ; attempt++)
        {
            Frame?.Invoke(true, req);
            var rsp = _port.Exchange(req, TimeoutMs);
            if (rsp != null) Frame?.Invoke(false, rsp);
            bool good = rsp != null && rsp.Length >= 5 && ModbusRtu.CrcOk(rsp) && rsp[0] == req[0];
            if (good)
            {
                if (rsp![1] == (expectFc | 0x80))
                    throw new ConfigException(rsp[2] == 4
                        ? "Bàn phím chưa ở chế độ cấu hình (exception 04) — dùng 'Bắt tay' hoặc 'Vào chế độ cấu hình'"
                        : $"Thiết bị báo lỗi Modbus: {ModbusRtu.ExceptionText(rsp[2])}");
                if (rsp[1] == expectFc && (expectLen < 0 || rsp.Length == expectLen)) return rsp;
            }
            if (attempt >= maxRetries)
                throw new ConfigException(rsp == null ? "Không có trả lời (hết giờ)" : "Khung trả lời sai/CRC lỗi");
        }
    }

    // ---------- Lệnh Modbus cơ bản ----------
    public ushort[] ReadRegisters(ushort addr, int count)
    {
        var rsp = Transact(ModbusRtu.ReadRegs(Station, 3, addr, (ushort)count), 3, 5 + count * 2);
        if (rsp[2] != count * 2) throw new ConfigException("Sai số byte trả về");
        var res = new ushort[count];
        for (int i = 0; i < count; i++) res[i] = (ushort)((rsp[3 + 2 * i] << 8) | rsp[4 + 2 * i]);
        return res;
    }

    public void WriteRegister(ushort addr, ushort val)
        => Transact(ModbusRtu.WriteReg(Station, addr, val), 6, 8);

    public void WriteRegisters(ushort addr, ReadOnlySpan<ushort> vals)
        => Transact(ModbusRtu.WriteRegs(Station, addr, vals), 16, 8);

    // ---------- Bắt tay ----------
    /// <summary>
    /// Bắt tay: gửi "đọc thanh ghi 100" mỗi ~100 ms tới khi bàn phím trả lời (cửa sổ 3 giây sau cấp nguồn).
    /// Trả về thông tin thiết bị; ném ConfigException nếu hết thời gian.
    /// </summary>
    public DeviceInfo Handshake(int totalTimeoutMs = 10000, int intervalMs = 100, CancellationToken ct = default)
    {
        var saveTimeout = TimeoutMs;
        TimeoutMs = Math.Min(saveTimeout, 120);
        try
        {
            var start = Environment.TickCount64;
            while (true)
            {
                ct.ThrowIfCancellationRequested();
                try
                {
                    var r = Transact(ModbusRtu.ReadRegs(Station, 3, InfoBase, 1), 3, 7, retries: 0);
                    if (((r[3] << 8) | r[4]) == InfoMagic) return ReadInfo();
                    throw new ConfigException("Thiết bị trả lời nhưng không phải bàn phím Panel (sai mã nhận dạng)");
                }
                catch (ConfigException ex) when (ex.Message.StartsWith("Không có trả lời") || ex.Message.StartsWith("Khung trả lời sai"))
                {
                    if (Environment.TickCount64 - start >= totalTimeoutMs)
                        throw new ConfigException("Bắt tay thất bại: bàn phím không trả lời. Cấp nguồn lại bàn phím rồi thử trong 3 giây đầu (9600 8N1, trạm 247), kiểm tra dây A/B.");
                    Thread.Sleep(intervalMs);
                }
            }
        }
        finally { TimeoutMs = saveTimeout; }
    }

    // ---------- Thông tin / chẩn đoán ----------
    public DeviceInfo ReadInfo()
    {
        var r = ReadRegisters(InfoBase, InfoCount);
        if (r[0] != InfoMagic) throw new ConfigException("Thiết bị không phải bàn phím Panel (sai mã nhận dạng)");
        return new DeviceInfo
        {
            Model = r[1], FwMajor = (byte)(r[2] >> 8), FwMinor = (byte)r[2], Patch = r[3], StructVersion = r[4],
            Serial = ((uint)r[5] << 16) | r[6], Caps = r[7], MaxPages = r[8], ImageBytes = r[10], MaxImageBytes = r[11],
            Status = r[12], LastResult = r[13], LastError = r[14], UptimeSec = r[15], FlashSeq = r[16], Revision = r[17],
        };
    }

    public Diagnostics ReadDiagnostics()
    {
        var r = ReadRegisters(DiagBase, DiagCount);
        return new Diagnostics
        {
            MasterOk = r[0], Timeouts = r[1], CrcErrors = r[2], Exceptions = r[3],
            LastErrorKind = (ushort)(r[4] >> 8), LastExceptionCode = (byte)r[4], SlaveServed = r[5],
        };
    }

    // ---------- Lệnh điều khiển ----------
    public static string ResultText(ushort result) => result switch
    {
        0 => "Thành công",
        0x20 => "Bàn phím chưa ở chế độ cấu hình",
        0x21 => "Lệnh không hợp lệ",
        1 => "Khối cấu hình sai mã nhận dạng",
        2 => "Phiên bản cấu trúc (schema) không hỗ trợ",
        3 => "Độ dài khối cấu hình sai",
        4 => "CRC32 sai",
        5 => "Thông số chung không hợp lệ",
        6 => "Cấu hình trang không hợp lệ",
        7 => "Cấu hình phím F không hợp lệ",
        8 => "Cấu hình LED không hợp lệ",
        9 => "Cấu hình cảnh báo không hợp lệ",
        10 => "Cấu hình mật khẩu không hợp lệ",
        0x12 => "Lỗi xoá flash",
        0x13 => "Lỗi ghi flash",
        0x14 => "Lỗi xác minh flash",
        _ => $"Mã kết quả {result} (0x{result:X})",
    };

    /// <summary>Gửi lệnh và không đọc kết quả (EXIT khởi động lại thiết bị nên thường không có kết quả đọc được).</summary>
    public void Command(ConfigCommand c) => WriteRegister(RegCmd, (ushort)c);

    /// <summary>Gửi lệnh, đọc thanh ghi kết quả (121), ném lỗi nếu khác 0.</summary>
    public void CommandChecked(ConfigCommand c)
    {
        Command(c);
        var result = ReadRegisters(RegResult, 1)[0];
        if (result != 0) throw new ConfigException($"{c}: {ResultText(result)}");
    }

    public void EnterConfigMode() => CommandChecked(ConfigCommand.EnterConfig);
    public void Identify() => CommandChecked(ConfigCommand.Identify);
    public void Apply() => CommandChecked(ConfigCommand.Apply);
    public void Reload() => CommandChecked(ConfigCommand.Reload);
    public void FactoryReset() => CommandChecked(ConfigCommand.Factory);
    public void ClearDiagnostics() => CommandChecked(ConfigCommand.ClearDiag);
    public void Exit() => Command(ConfigCommand.Exit);

    // ---------- Khối cấu hình ----------
    public static ushort[] ToRegisters(byte[] image)
    {
        var regs = new ushort[(image.Length + 1) / 2];
        for (int i = 0; i < regs.Length; i++)
        {
            int hi = image[2 * i], lo = 2 * i + 1 < image.Length ? image[2 * i + 1] : 0;
            regs[i] = (ushort)((hi << 8) | lo);
        }
        return regs;
    }

    public static byte[] FromRegisters(ushort[] regs, int length)
    {
        var b = new byte[length];
        for (int i = 0; i < length; i++) b[i] = (byte)(i % 2 == 0 ? regs[i / 2] >> 8 : regs[i / 2]);
        return b;
    }

    /// <summary>Đọc ngược khối cấu hình đang chạy (sao lưu). Kiểm tra magic + CRC32.</summary>
    public byte[] ReadConfig(IProgress<int>? progress = null)
    {
        var info = ReadInfo();
        int len = info.ImageBytes;
        int regsTotal = (len + 1) / 2;
        var regs = new ushort[regsTotal];
        for (int off = 0; off < regsTotal; off += ChunkRegs)
        {
            int n = Math.Min(ChunkRegs, regsTotal - off);
            ReadRegisters((ushort)(BufBase + off), n).CopyTo(regs, off);
            progress?.Report(Math.Min(100, (off + n) * 100 / regsTotal));
        }
        var img = FromRegisters(regs, len);
        if (!img.AsSpan(0, 4).SequenceEqual(RuntimeCompiler.Magic)) throw new ConfigException("Khối cấu hình đọc về sai mã nhận dạng");
        if (len == RuntimeCompiler.ImageSize &&
            BinaryPrimitives.ReadUInt32LittleEndian(img.AsSpan(RuntimeCompiler.CrcOffset)) != Crc32.Compute(img.AsSpan(0, RuntimeCompiler.CrcOffset)))
            throw new ConfigException("Khối cấu hình đọc về sai CRC32 (có thể bàn phím đang dùng mặc định)");
        return img;
    }

    /// <summary>Ghi khối cấu hình → đọc lại so sánh → (tuỳ chọn) APPLY. Báo tiến trình 0…100.</summary>
    public void Download(byte[] image, IProgress<int>? progress = null, bool apply = true)
    {
        var info = ReadInfo();
        if (!info.InConfigMode)
            throw new ConfigException("Bàn phím chưa ở chế độ cấu hình — dùng 'Bắt tay' (cấp nguồn lại) hoặc 'Vào chế độ cấu hình'");
        if (info.StructVersion != RuntimeCompiler.FormatVersion)
            throw new ConfigException($"Phiên bản cấu trúc không khớp: bàn phím v{info.StructVersion}, PC app v{RuntimeCompiler.FormatVersion}");
        if (image.Length != info.ImageBytes)
            throw new ConfigException($"Khối cấu hình {image.Length} B, bàn phím cần {info.ImageBytes} B");

        var regs = ToRegisters(image);
        int total = regs.Length * 2, done = 0;
        for (int off = 0; off < regs.Length; off += ChunkRegs)
        {
            int n = Math.Min(ChunkRegs, regs.Length - off);
            WriteRegisters((ushort)(BufBase + off), regs.AsSpan(off, n));
            done += n; progress?.Report(done * 100 / total);
        }
        for (int off = 0; off < regs.Length; off += ChunkRegs)
        {
            int n = Math.Min(ChunkRegs, regs.Length - off);
            var back = ReadRegisters((ushort)(BufBase + off), n);
            for (int i = 0; i < n; i++)
                if (back[i] != regs[off + i])
                    throw new ConfigException($"Đọc lại không khớp tại thanh ghi đệm {off + i} — ghi lỗi");
            done += n; progress?.Report(Math.Min(99, done * 100 / total));
        }
        if (!apply) { progress?.Report(100); return; }
        Apply();
        progress?.Report(100);
    }

    // ---------- Thử phím / màn / LED / còi (thanh ghi slave) ----------
    public LiveState ReadLive()
    {
        var a = ReadRegisters(RegKeyCode, 7);
        var b = ReadRegisters(RegTopHi, 7);   // 10..16
        return new LiveState
        {
            KeyCode = a[0], KeyCount = a[1], Flags = a[4],
            KeysDown = a[5] | ((uint)(a[6] & 0xFF) << 16),
            Top = (int)(((uint)b[0] << 16) | b[1]), Bottom = (int)(((uint)b[2] << 16) | b[3]), Led = b[4],
        };
    }

    /// <summary>Ghi hai hàng số (int32, 10–13). Tắt chế độ đoạn thô.</summary>
    public void WriteDisplay(int top, int bottom)
    {
        WriteRegisters(RegTopHi, new ushort[] { (ushort)((uint)top >> 16), (ushort)top, (ushort)((uint)bottom >> 16), (ushort)bottom });
        WriteRegister(RegFmt, 0);
    }

    /// <summary>Ghi đoạn thô cho 8 digit (bit0 = a … bit6 = g, bit7 = chấm) và bật chế độ đoạn thô.</summary>
    public void WriteRawSegments(ReadOnlySpan<byte> segs)
    {
        var v = new ushort[8];
        for (int i = 0; i < 8 && i < segs.Length; i++) v[i] = segs[i];
        WriteRegisters(RegRaw0, v);
        WriteRegister(RegFmt, FmtRaw);
    }

    public void WriteLeds(byte steady, byte blink = 0) => WriteRegister(RegLed, (ushort)(steady | (blink << 8)));
    public void Beep(bool longBeep = false, int brightness = 5)
        => WriteRegister(RegCtrl, (ushort)((longBeep ? CtrlBeepLong : CtrlBeepShort) | Math.Clamp(brightness, 0, 7)));
    public void SetBrightness(int level) => WriteRegister(RegCtrl, (ushort)(Math.Clamp(level, 0, 7)));
}

/// <summary>Bàn phím ảo trong bộ nhớ: chạy đúng giao thức cấu hình để thử khi chưa có phần cứng.</summary>
public sealed class SimulatedDevice : IModbusPort
{
    public string Name => "Thiết bị ảo (mô phỏng)";
    public ushort MaxImageBytes { get; set; } = 1984;
    public byte[]? AppliedImage { get; private set; }
    public int IdentifyCount { get; private set; }
    public bool Exited { get; private set; }
    public bool InConfigMode { get; set; } = true;
    public int DropNext { get; set; }          // số khung bỏ qua tiếp theo (thử timeout)
    public bool CorruptWrites { get; set; }    // làm hỏng dữ liệu ghi (thử đọc lại)
    public int ClearDiagCount { get; private set; }
    public ushort[] Diag { get; } = { 12, 1, 2, 0, 0, 7 };
    public uint KeysDown { get; set; }
    public ushort KeyCount { get; set; }

    readonly ushort[] _buf = new ushort[ConfigClient.BufRegs];
    readonly ushort[] _slave = new ushort[32];
    ushort _result, _lastCmd;
    int _revision;

    public void Open() { }
    public void Close() { }
    public void Dispose() { }

    public byte[]? Exchange(byte[] req, int timeoutMs)
    {
        if (DropNext > 0) { DropNext--; return null; }
        if (req.Length < 8 || !ModbusRtu.CrcOk(req)) return null;
        if (req[0] != ConfigClient.DefaultStation) return null;
        byte fc = req[1];
        ushort addr = (ushort)((req[2] << 8) | req[3]);
        switch (fc)
        {
            case 3:
            case 4:
            {
                int n = (req[4] << 8) | req[5];
                if (n < 1 || n > 125) return Exc(fc, 3);
                var pdu = new byte[3 + 2 * n];
                pdu[0] = req[0]; pdu[1] = fc; pdu[2] = (byte)(2 * n);
                for (int i = 0; i < n; i++)
                {
                    if (!TryRead((ushort)(addr + i), out var v)) return Exc(fc, 2);
                    pdu[3 + 2 * i] = (byte)(v >> 8); pdu[4 + 2 * i] = (byte)v;
                }
                return ModbusRtu.WithCrc(pdu);
            }
            case 6:
            {
                ushort v = (ushort)((req[4] << 8) | req[5]);
                byte err = TryWrite(addr, v);
                return err != 0 ? Exc(fc, err) : req;
            }
            case 16:
            {
                int n = (req[4] << 8) | req[5];
                if (n < 1 || n > 123 || req[6] != 2 * n || req.Length != 9 + 2 * n) return Exc(fc, 3);
                for (int i = 0; i < n; i++)
                {
                    ushort v = (ushort)((req[7 + 2 * i] << 8) | req[8 + 2 * i]);
                    byte err = TryWrite((ushort)(addr + i), v);
                    if (err != 0) return Exc(fc, err);
                }
                return ModbusRtu.WithCrc(req.AsSpan(0, 6).ToArray());
            }
            default: return Exc(fc, 1);
        }
    }

    static byte[] Exc(byte fc, byte code) => ModbusRtu.WithCrc(new[] { ConfigClient.DefaultStation, (byte)(fc | 0x80), code });

    bool TryRead(ushort a, out ushort v)
    {
        v = 0;
        if (a >= ConfigClient.InfoBase && a < ConfigClient.InfoBase + ConfigClient.InfoCount)
        {
            ushort caps = (ushort)(0x06 | (InConfigMode ? 0x10 : 0));
            ushort status = (ushort)((AppliedImage != null ? 1 : 0) | (InConfigMode ? 0x08 : 0) | 0x10);
            ushort[] info = { ConfigClient.InfoMagic, 1, 0x0100, 2, RuntimeCompiler.FormatVersion, 0x1234, 0x5678, caps, 32, 0x0A08,
                              RuntimeCompiler.ImageSize, MaxImageBytes, status, _lastCmd, 0, 42, (ushort)(AppliedImage != null ? 1 : 0), (ushort)_revision };
            v = info[a - ConfigClient.InfoBase]; return true;
        }
        if (a == ConfigClient.RegCmd) { v = _lastCmd; return true; }
        if (a == ConfigClient.RegResult) { v = _result; return true; }
        if (a >= ConfigClient.DiagBase && a < ConfigClient.DiagBase + ConfigClient.DiagCount) { v = Diag[a - ConfigClient.DiagBase]; return true; }
        if (a >= ConfigClient.BufBase && a < ConfigClient.BufBase + _buf.Length)
        {
            if (AppliedImage != null && a - ConfigClient.BufBase < (AppliedImage.Length + 1) / 2 && !_bufDirty)
                v = ConfigClient.ToRegisters(AppliedImage)[a - ConfigClient.BufBase];
            else v = _buf[a - ConfigClient.BufBase];
            return true;
        }
        if (a == ConfigClient.RegKeyCount) { v = KeyCount; return true; }
        if (a == ConfigClient.RegKeysLo) { v = (ushort)KeysDown; return true; }
        if (a == ConfigClient.RegKeysExt) { v = (ushort)((KeysDown >> 16) & 0xFF); return true; }
        if (a < _slave.Length) { v = _slave[a]; return true; }
        return false;
    }

    bool _bufDirty;

    /// <summary>0 = OK, khác 0 = mã exception.</summary>
    byte TryWrite(ushort a, ushort v)
    {
        if (a >= ConfigClient.BufBase && a < ConfigClient.BufBase + _buf.Length)
        {
            if (!InConfigMode) return 4;
            _buf[a - ConfigClient.BufBase] = CorruptWrites ? (ushort)(v ^ 1) : v; _bufDirty = true; return 0;
        }
        if (a == ConfigClient.RegCmd)
        {
            _lastCmd = v;
            bool needCfg = (ConfigCommand)v is ConfigCommand.Apply or ConfigCommand.Reload or ConfigCommand.Factory;
            if (needCfg && !InConfigMode) { _result = 0x20; return 0; }
            switch ((ConfigCommand)v)
            {
                case ConfigCommand.Apply: Apply(); break;
                case ConfigCommand.Identify: IdentifyCount++; _result = 0; break;
                case ConfigCommand.Exit: Exited = true; _result = 0; break;
                case ConfigCommand.Factory: AppliedImage = null; _bufDirty = true; _result = 0; break;
                case ConfigCommand.Reload: _bufDirty = false; _result = 0; break;
                case ConfigCommand.EnterConfig: InConfigMode = true; _result = 0; break;
                case ConfigCommand.ClearDiag: Array.Clear(Diag); ClearDiagCount++; _result = 0; break;
                default: _result = 0x21; break;
            }
            return 0;
        }
        if (a >= ConfigClient.RegKeyCode && a < _slave.Length && a != ConfigClient.RegKeyCount && a != ConfigClient.RegKeysLo && a != ConfigClient.RegKeysExt)
        { _slave[a] = v; return 0; }
        return 2;
    }

    public ushort GetSlaveRegister(int a) => _slave[a];

    void Apply()
    {
        var img = ConfigClient.FromRegisters(_buf, RuntimeCompiler.ImageSize);
        if (!img.AsSpan(0, 4).SequenceEqual(RuntimeCompiler.Magic)) { _result = 1; return; }
        if (BinaryPrimitives.ReadUInt16LittleEndian(img.AsSpan(4)) != RuntimeCompiler.FormatVersion) { _result = 2; return; }
        if (BinaryPrimitives.ReadUInt16LittleEndian(img.AsSpan(6)) != RuntimeCompiler.ImageSize) { _result = 3; return; }
        if (BinaryPrimitives.ReadUInt32LittleEndian(img.AsSpan(RuntimeCompiler.CrcOffset)) != Crc32.Compute(img.AsSpan(0, RuntimeCompiler.CrcOffset))) { _result = 4; return; }
        AppliedImage = img; _bufDirty = false; _revision++; _result = 0;
    }
}
