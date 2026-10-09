using System.Buffers.Binary;

namespace PanelKbd.Core;

public static class Crc32
{
    static readonly uint[] Table = Build();
    static uint[] Build()
    {
        var t = new uint[256];
        for (uint i = 0; i < 256; i++)
        {
            uint c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }

    public static uint Compute(ReadOnlySpan<byte> data)
    {
        uint c = 0xFFFFFFFFu;
        foreach (var b in data) c = Table[(c ^ b) & 0xFF] ^ (c >> 8);
        return ~c;
    }
}

public sealed class CompileResult
{
    public CheckResult Check { get; init; } = new();
    public byte[]? Image { get; init; }
    public uint Crc { get; init; }
    public bool Ok => Image != null;
}

/// <summary>
/// Quy đổi địa chỉ theo hồ sơ rồi tạo khối cấu hình "PKRT" 1918 byte (xem docs/RUNTIME_FORMAT.md,
/// nguồn chuẩn firmware/App/Inc/rtcfg.h).
/// </summary>
public static class RuntimeCompiler
{
    /// <summary>Phiên bản cấu trúc khối (schema) — phải bằng RT_SCHEMA của firmware.</summary>
    public const int FormatVersion = 1;
    public const int ImageSize = 1918, CrcOffset = 1914;
    public const int CommSize = 22, SecSize = 8, PageSize = 50, ObjSize = 24, FKeySize = 8, LedSize = 8, AlarmSize = 8;
    public const int MaxFKeys = 10, MaxLeds = 8, MaxAlarms = 16;
    const int OffComm = 12, OffSec = 34, OffPage = 42, OffFKey = 1642, OffLed = 1722, OffAlarm = 1786;
    public static readonly byte[] Magic = { (byte)'P', (byte)'K', (byte)'R', (byte)'T' };
    static readonly int[] Bauds = { 2400, 4800, 9600, 19200, 38400, 57600, 115200 };

    public static CompileResult Compile(ProjectFile p, PlcProfile profile, int revision = 0)
    {
        var chk = Validator.Check(p, profile);
        if (chk.HasErrors) return new CompileResult { Check = chk };
        var img = Build(p, profile, revision);
        return new CompileResult { Check = chk, Image = img, Crc = BinaryPrimitives.ReadUInt32LittleEndian(img.AsSpan(CrcOffset, 4)) };
    }

    /// <summary>Dùng cho kiểm tra: không kiểm tra lỗi ngữ nghĩa, chỉ cần quy đổi được.</summary>
    public static bool TryBuild(ProjectFile p, PlcProfile profile, out byte[]? image, out string error)
    {
        try { image = Build(p, profile, 0); error = ""; return true; }
        catch (Exception ex) { image = null; error = ex.Message; return false; }
    }

    static ModbusRef R(string addr, PlcProfile prof, CommSettings c)
    {
        var r = AddressResolver.Resolve(addr, prof, c);
        if (!r.Ok) throw new InvalidOperationException($"{addr}: {r.Error}");
        return r.Ref;
    }

    /// <summary>Vùng Modbus của PC app → RT_AREA_* của firmware (0 giữ, 1 đầu vào, 2 cuộn dây, 3 đầu vào rời).</summary>
    static byte Area(ModbusTable t) => t switch
    {
        ModbusTable.Holding => 0, ModbusTable.Input => 1, ModbusTable.Coil => 2, _ => 3,
    };

    /// <summary>RT_DT_*: 0 u16, 1 i16, 2 u32, 3 i32.</summary>
    static byte Dt(DataType t) => t switch
    {
        DataType.UInt16 => 0, DataType.Int16 => 1, DataType.UInt32 => 2, _ => 3,
    };

    static byte Order(WordOrder o) => (byte)(o == WordOrder.ABCD ? 1 : 2);

    static byte[] Build(ProjectFile p, PlcProfile prof, int revision)
    {
        var c = p.Comm;
        bool master = c.Mode == OperationMode.Master;
        var b = new byte[ImageSize];
        var s = b.AsSpan();

        Magic.CopyTo(s);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(4), FormatVersion);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(6), ImageSize);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(8), (ushort)revision);

        // --- comm (22 byte) ---
        int o = OffComm;
        int pages = master ? Math.Min(p.Pages.Count, ProjectFile.MaxPages) : 0;
        var sec = p.Security;
        b[o + 0] = (byte)(master ? 0 : 1);
        b[o + 2] = (byte)c.PlcStation; b[o + 3] = (byte)c.KeyboardStation;
        int bi = Array.IndexOf(Bauds, c.Baud);
        b[o + 4] = (byte)(bi < 0 ? 2 : bi);
        b[o + 5] = (byte)c.Parity; b[o + 6] = (byte)c.StopBits;
        b[o + 7] = Order(c.WordOrder);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(o + 8), (ushort)c.TimeoutMs);
        b[o + 10] = (byte)c.Retries;
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(o + 11), (ushort)c.PollMs);
        bool k3 = master && p.Keys.Any(k => k.Id == "K3" && k.Present);
        b[o + 13] = (byte)((c.ReadBackAfterWrite ? 0x01 : 0) | (k3 ? 0x02 : 0));   // RT_CF_VERIFY | RT_CF_EXTKEYS
        b[o + 14] = (byte)c.Brightness;
        b[o + 15] = (byte)(c.BuzzerClick ? 1 : 0);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(o + 16), (ushort)Math.Clamp(sec.RunTimeoutSec, 0, 65535));
        b[o + 18] = (byte)Math.Max(0, sec.RunPage - 1);
        b[o + 19] = (byte)pages;

        // --- sec (8 byte) ---
        o = OffSec;
        ushort.TryParse(sec.OperatorPassword, out var op); ushort.TryParse(sec.TechPassword, out var tp);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(o), op);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(o + 2), tp);

        // Ô chưa dùng phải có bit = 0xFF, mục cảnh báo "không dùng" cũng vậy.
        for (int i = 0; i < 32; i++) { s[OffPage + i * PageSize + 8] = 0xFF; s[OffPage + i * PageSize + ObjSize + 8] = 0xFF; }
        for (int i = 0; i < MaxFKeys; i++) s[OffFKey + i * FKeySize + 4] = 0xFF;
        for (int i = 0; i < MaxLeds; i++) s[OffLed + i * LedSize + 3] = 0xFF;
        for (int i = 0; i < MaxAlarms; i++) s[OffAlarm + i * AlarmSize + 3] = 0xFF;

        if (master)
        {
            for (int i = 0; i < pages; i++)
            {
                var pg = p.Pages[i];
                o = OffPage + i * PageSize;
                if (p.Layout.HasUpper) WriteCell(s.Slice(o, ObjSize), pg.Upper, prof, c);
                if (p.Layout.HasLower && !(p.Layout.HasUpper && pg.Upper.Span8))
                    WriteCell(s.Slice(o + ObjSize, ObjSize), pg.Lower, prof, c);
            }
            // F1, F2, K3 -> fkey[0..2]; ▲▼ là phím điều hướng cố định của firmware.
            int fk = 0;
            foreach (var k in p.Keys)
            {
                if (k.Id is "UP" or "DN") continue;
                if (fk >= MaxFKeys) break;
                WriteFKey(s.Slice(OffFKey + fk * FKeySize, FKeySize), k, prof, c);
                fk++;
            }
            for (int i = 0; i < p.Leds.Count && i < MaxLeds; i++)
                WriteLed(s.Slice(OffLed + i * LedSize, LedSize), p.Leds[i], prof, c);
        }

        BinaryPrimitives.WriteUInt32LittleEndian(s.Slice(CrcOffset), Crc32.Compute(s.Slice(0, CrcOffset)));
        return b;
    }

    static void WriteFKey(Span<byte> s, KeyConfig k, PlcProfile prof, CommSettings c)
    {
        s[4] = 0xFF;
        if (!k.Present) return;
        s[0] = (byte)(k.Action switch
        {
            KeyAction.Hold => 1, KeyAction.Toggle => 2, KeyAction.Set => 3, KeyAction.Reset => 4,
            KeyAction.WriteConst => 5, KeyAction.GotoPage => 6, KeyAction.NextPage => 7, KeyAction.PrevPage => 8, _ => 0,
        });
        if (k.Action == KeyAction.GotoPage) BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(5), (ushort)Math.Max(0, k.TargetPage - 1));
        if (!k.NeedsAddress) return;
        var m = R(k.Address, prof, c);
        // fkey.area: 0 cuộn dây, 1 bit trong thanh ghi giữ, 2 cả thanh ghi giữ
        s[1] = (byte)(m.Table == ModbusTable.Coil ? 0 : m.HasBit ? 1 : 2);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(2), (ushort)m.Address);
        s[4] = (byte)(m.HasBit ? m.Bit : 0xFF);
        if (k.Action == KeyAction.WriteConst) BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(5), (ushort)k.Const);
    }

    static void WriteLed(Span<byte> s, LedConfig l, PlcProfile prof, CommSettings c)
    {
        s[3] = 0xFF;
        if (!l.Present) return;
        s[4] = (byte)(l.Mode switch { LedMode.Steady => 1, LedMode.Blink => 2, LedMode.Comm => 3, _ => 0 });
        s[5] = (byte)(l.Invert ? 1 : 0);
        if (!l.NeedsAddress) return;
        var m = R(l.Address, prof, c);
        s[0] = Area(m.Table);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(1), (ushort)m.Address);
        s[3] = (byte)(m.HasBit ? m.Bit : 0xFF);
    }

    static void WriteCell(Span<byte> s, Cell cell, PlcProfile prof, CommSettings c)
    {
        s[8] = 0xFF;
        s[0] = (byte)cell.Type;                // None, Display, Input, Label, BitState trùng RT_OBJ_*
        switch (cell.Type)
        {
            case CellType.None: return;
            case CellType.Label:
                SevenSeg.LabelBytes(cell.Label).CopyTo(s.Slice(10));
                return;
        }
        var m = R(cell.Address, prof, c);
        s[1] = Area(m.Table);
        BinaryPrimitives.WriteUInt16LittleEndian(s.Slice(2), (ushort)m.Address);
        s[4] = Dt(cell.DataType);
        s[5] = 0;                               // theo thứ tự word chung
        s[6] = (byte)cell.Decimals;
        s[7] = (byte)cell.PasswordLevel;
        s[8] = (byte)(m.HasBit ? m.Bit : 0xFF);
        s[9] = (byte)(cell.Span8 && cell.IsUpper ? 1 : 0);
        if (cell.Type == CellType.BitState)
        {
            SevenSeg.LabelBytes(cell.OnLabel).CopyTo(s.Slice(10));
            SevenSeg.LabelBytes(cell.OffLabel).CopyTo(s.Slice(14));
        }
        else
        {
            BinaryPrimitives.WriteInt32LittleEndian(s.Slice(14), cell.Min);
            BinaryPrimitives.WriteInt32LittleEndian(s.Slice(18), cell.Max);
        }
        BinaryPrimitives.WriteInt16LittleEndian(s.Slice(22), 1000);
    }
}
