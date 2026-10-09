using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace PanelKbd.Core;

public enum ModbusTable { Coil, Discrete, Holding, Input }
public enum AreaKind { Word, Bit }

public static class ModbusTableExt
{
    public static bool IsBitTable(this ModbusTable t) => t is ModbusTable.Coil or ModbusTable.Discrete;
    public static bool IsWritable(this ModbusTable t) => t is ModbusTable.Coil or ModbusTable.Holding;
    public static string Display(this ModbusTable t) => t switch
    {
        ModbusTable.Coil => "Coil (FC01/05)",
        ModbusTable.Discrete => "Discrete input (FC02)",
        ModbusTable.Holding => "Holding reg (FC03/06)",
        _ => "Input reg (FC04)",
    };
}

/// <summary>Một vùng nhớ PLC (D, M, X, Y...) và cách nó nằm trong bảng Modbus.</summary>
public sealed class PlcArea
{
    public string Name { get; set; } = "";
    public AreaKind Kind { get; set; } = AreaKind.Word;
    public ModbusTable Table { get; set; } = ModbusTable.Holding;
    /// <summary>10 = thập phân, 8 = bát phân (X/Y của PLC Mitsubishi, Delta).</summary>
    public int Radix { get; set; } = 10;
    /// <summary>Địa chỉ Modbus (0-based) của phần tử đầu vùng (hoặc của địa chỉ người dùng đặt).</summary>
    public int ModbusStart { get; set; }
    public int Min { get; set; }
    public int Max { get; set; }
    /// <summary>Số đơn vị địa chỉ PLC trên một thanh ghi Modbus (2 với V của Siemens: VW0, VW2...).</summary>
    public int Stride { get; set; } = 1;
    public string Desc { get; set; } = "";
}

public sealed class PlcProfile
{
    public string Id { get; set; } = "";
    public string Name { get; set; } = "";
    public string Vendor { get; set; } = "";
    /// <summary>Đã kiểm tra bằng PLC thật chưa (PC-21).</summary>
    public bool Verified { get; set; }
    /// <summary>Nguồn tài liệu: tên sổ tay, trang.</summary>
    public string Source { get; set; } = "";
    public string Notes { get; set; } = "";
    public WordOrder DefaultWordOrder { get; set; } = WordOrder.CDAB;
    /// <summary>Timeout tối thiểu nên dùng (ms): PLC chỉ trả lời trong pha truyền thông của vòng quét. 0 = không yêu cầu.</summary>
    public int MinTimeoutMs { get; set; }
    public bool NeedsUserOffset { get; set; }
    public string OffsetLabel { get; set; } = "";
    /// <summary>Hồ sơ Modbus chung: có chọn kiểu đánh địa chỉ 0/1/40001.</summary>
    public bool IsGeneric { get; set; }
    public List<PlcArea> Areas { get; set; } = new();

    [JsonIgnore] public string Display => Verified ? Name : Name + "  (chưa kiểm tra)";

    public static readonly JsonSerializerOptions Json = new()
    {
        WriteIndented = true,
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        Converters = { new JsonStringEnumConverter() },
    };

    public static PlcProfile Load(string path)
        => JsonSerializer.Deserialize<PlcProfile>(File.ReadAllText(path), Json)
           ?? throw new InvalidDataException("Hồ sơ rỗng: " + path);

    /// <summary>Nạp mọi *.json trong các thư mục; lỗi từng file được trả về, không dừng cả danh sách.</summary>
    public static (List<PlcProfile> Profiles, List<string> Errors) LoadFolders(params string[] folders)
    {
        var list = new List<PlcProfile>(); var errs = new List<string>();
        foreach (var f in folders.Where(Directory.Exists))
            foreach (var file in Directory.GetFiles(f, "*.json").OrderBy(x => x))
            {
                try
                {
                    var p = Load(file);
                    if (string.IsNullOrWhiteSpace(p.Id)) throw new InvalidDataException("thiếu id");
                    list.RemoveAll(x => x.Id == p.Id); // thư mục sau ghi đè thư mục trước
                    list.Add(p);
                }
                catch (Exception ex) { errs.Add($"{Path.GetFileName(file)}: {ex.Message}"); }
            }
        return (list.OrderBy(p => p.IsGeneric ? 0 : 1).ThenBy(p => p.Name).ToList(), errs);
    }
}

public readonly record struct ModbusRef(ModbusTable Table, int Address, int Bit, PlcArea? Area)
{
    public bool HasBit => Bit >= 0;
    /// <summary>Kiểu 4xxxx để thợ đối chiếu.</summary>
    public string Classic
    {
        get
        {
            int p = Table switch { ModbusTable.Coil => 0, ModbusTable.Discrete => 1, ModbusTable.Input => 3, _ => 4 };
            return $"{p}{Address + 1:D4}";
        }
    }
    public override string ToString() => $"{Table.Display()} {Address} (0x{Address:X4}){(HasBit ? $" bit {Bit}" : "")}";
}

public sealed record ResolveResult(bool Ok, string Error, ModbusRef Ref)
{
    public static ResolveResult Fail(string e) => new(false, e, default);
    public static ResolveResult Success(ModbusRef r) => new(true, "", r);
}

public static class AddressResolver
{
    static readonly Regex Pat = new(@"^([A-Za-z]+)\s*(\d+)(?:\.(\d{1,2}))?$", RegexOptions.Compiled);
    static readonly Regex Classic = new(@"^([0134])(\d{4,5})$", RegexOptions.Compiled);
    /// <summary>Địa chỉ Modbus tuyệt đối (0-based, đúng như trên dây), dùng được cho mọi PLC: HR:4096, IR:7, C:16, DI:5, HR:0x1000.3</summary>
    static readonly Regex Abs = new(@"^(HR|IR|C|DI)\s*:\s*(0x[0-9A-Fa-f]+|\d+)(?:\.(\d{1,2}))?$", RegexOptions.Compiled | RegexOptions.IgnoreCase);

    public static string FormatAbsolute(ModbusRef r)
    {
        string p = r.Table switch { ModbusTable.Coil => "C", ModbusTable.Discrete => "DI", ModbusTable.Input => "IR", _ => "HR" };
        return $"{p}:{r.Address}" + (r.HasBit ? $".{r.Bit}" : "");
    }

    static ResolveResult ResolveAbsolute(Match m)
    {
        var table = m.Groups[1].Value.ToUpperInvariant() switch { "C" => ModbusTable.Coil, "DI" => ModbusTable.Discrete, "IR" => ModbusTable.Input, _ => ModbusTable.Holding };
        var t = m.Groups[2].Value;
        int addr;
        try { addr = t.StartsWith("0x", StringComparison.OrdinalIgnoreCase) ? Convert.ToInt32(t[2..], 16) : int.Parse(t); }
        catch { return ResolveResult.Fail("Số quá lớn"); }
        if (addr is < 0 or > 65535) return ResolveResult.Fail("Địa chỉ Modbus phải 0…65535");
        int bit = -1;
        if (m.Groups[3].Success)
        {
            if (table.IsBitTable()) return ResolveResult.Fail("Coil / đầu vào bit không có '.bit'");
            bit = int.Parse(m.Groups[3].Value);
            if (bit > 15) return ResolveResult.Fail("Số bit phải 0…15");
        }
        return ResolveResult.Success(new ModbusRef(table, addr, bit, null));
    }

    /// <summary>Đổi địa chỉ tuyệt đối về ký hiệu ngắn của hồ sơ (D100, Y10…); null nếu không có vùng nào chứa nó.</summary>
    public static string? TryShort(ModbusRef r, PlcProfile profile, CommSettings comm)
    {
        if (profile.IsGeneric && comm.AddressStyle == AddressStyle.Classic && !r.HasBit) return r.Classic;
        foreach (var a in profile.Areas.Where(a => a.Table == r.Table))
        {
            int rel = r.Address - a.ModbusStart;
            if (rel < 0) continue;
            int baseN = profile.NeedsUserOffset ? comm.UserOffset : a.Min;
            int num = baseN + rel * a.Stride;
            if (num < a.Min || num > a.Max) continue;
            if (profile.IsGeneric && comm.AddressStyle == AddressStyle.Base1) num += 1;
            if (r.HasBit && a.Kind == AreaKind.Bit) continue;
            return a.Name + ToRadix(num, a.Radix) + (r.HasBit ? $".{r.Bit}" : "");
        }
        return null;
    }

    public static ResolveResult Resolve(string? text, PlcProfile profile, CommSettings comm)
    {
        text = (text ?? "").Trim();
        if (text.Length == 0) return ResolveResult.Fail("Chưa nhập địa chỉ");

        if (profile.IsGeneric && comm.AddressStyle == AddressStyle.Classic)
        {
            var cm = Classic.Match(text);
            if (cm.Success)
            {
                var table = cm.Groups[1].Value switch { "0" => ModbusTable.Coil, "1" => ModbusTable.Discrete, "3" => ModbusTable.Input, _ => ModbusTable.Holding };
                int n = int.Parse(cm.Groups[2].Value);
                if (n < 1 || n > 65536) return ResolveResult.Fail("Địa chỉ kiểu 4xxxx ngoài 1…65536");
                return ResolveResult.Success(new ModbusRef(table, n - 1, -1, null));
            }
        }

        var am = Abs.Match(text);
        if (am.Success) return ResolveAbsolute(am);

        var m = Pat.Match(text);
        if (!m.Success) return ResolveResult.Fail(profile.IsGeneric && comm.AddressStyle == AddressStyle.Classic
            ? "Dạng 40001 / 00001 / 30001 / 10001 (hoặc HR100.3)"
            : "Sai cú pháp, ví dụ D100 hoặc D100.3");

        string name = m.Groups[1].Value;
        var cands = profile.Areas.Where(a => a.Name.Equals(name, StringComparison.OrdinalIgnoreCase)).ToList();
        if (cands.Count == 0)
            return ResolveResult.Fail($"Hồ sơ '{profile.Name}' không có vùng '{name.ToUpperInvariant()}'. Có: " +
                string.Join(", ", profile.Areas.Select(a => a.Name).Distinct()));

        var digits = m.Groups[2].Value;
        int radix = cands[0].Radix;
        int num = 0;
        foreach (char ch in digits)
        {
            int d = ch - '0';
            if (d >= radix) return ResolveResult.Fail($"Vùng {name.ToUpperInvariant()} dùng cơ số {radix}: chữ số '{ch}' không hợp lệ");
            num = checked(num * radix + d);
            if (num > 10_000_000) return ResolveResult.Fail("Số quá lớn");
        }

        if (profile.IsGeneric && comm.AddressStyle == AddressStyle.Base1)
        {
            if (num < 1) return ResolveResult.Fail("Kiểu đánh địa chỉ từ 1: số phải ≥ 1");
            num -= 1;
        }

        var area = cands.FirstOrDefault(a => num >= a.Min && num <= a.Max);
        if (area == null)
        {
            var lo = cands.Min(a => a.Min); var hi = cands.Max(a => a.Max);
            return ResolveResult.Fail($"{name.ToUpperInvariant()}{digits} ngoài vùng {name.ToUpperInvariant()}{ToRadix(lo, radix)}…{name.ToUpperInvariant()}{ToRadix(hi, radix)}");
        }

        int baseN = profile.NeedsUserOffset ? comm.UserOffset : area.Min;
        int rel = num - baseN;
        if (rel < 0) return ResolveResult.Fail($"Thấp hơn địa chỉ bắt đầu {baseN} đã đặt trong 'Kết nối PLC'");
        if (rel % area.Stride != 0) return ResolveResult.Fail($"Địa chỉ phải chia hết cho {area.Stride} (vùng tính theo byte)");
        int addr = area.ModbusStart + rel / area.Stride;
        if (addr > 65535) return ResolveResult.Fail("Địa chỉ Modbus vượt 65535");

        int bit = -1;
        if (m.Groups[3].Success)
        {
            if (area.Kind == AreaKind.Bit) return ResolveResult.Fail($"{name.ToUpperInvariant()} là vùng bit, không có '.bit'");
            bit = int.Parse(m.Groups[3].Value);
            if (bit > 15) return ResolveResult.Fail("Số bit phải 0…15");
        }
        return ResolveResult.Success(new ModbusRef(area.Table, addr, bit, area));
    }

    static string ToRadix(int n, int radix) => radix == 8 ? Convert.ToString(n, 8) : n.ToString();
}

/// <summary>Hồ sơ PLC và cài đặt hiện hành, để các ô địa chỉ đổi qua lại giữa ký hiệu ngắn và địa chỉ Modbus tuyệt đối.</summary>
public static class AddressContext
{
    public static PlcProfile? Profile { get; set; }
    public static CommSettings? Comm { get; set; }

    public static string ToAbsolute(string address)
    {
        if (Profile == null || Comm == null || string.IsNullOrWhiteSpace(address)) return "";
        var r = AddressResolver.Resolve(address, Profile, Comm);
        return r.Ok ? AddressResolver.FormatAbsolute(r.Ref) : "";
    }

    /// <summary>Ô tuyệt đối vừa sửa → giá trị lưu ở ô ký hiệu ngắn (nếu có vùng khớp), nếu không giữ dạng tuyệt đối.</summary>
    public static string FromAbsolute(string text)
    {
        text = (text ?? "").Trim();
        if (Profile == null || Comm == null || text.Length == 0) return text;
        var r = AddressResolver.Resolve(text, Profile, Comm);
        if (!r.Ok) return text;
        return AddressResolver.TryShort(r.Ref, Profile, Comm) ?? AddressResolver.FormatAbsolute(r.Ref);
    }
}
