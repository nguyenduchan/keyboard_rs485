using PanelKbd.Core;
using Xunit;

public class CoreTests
{
    static PlcProfile Load(string id) => PlcProfile.Load(Path.Combine(AppContext.BaseDirectory, "profiles", id + ".json"));
    static CommSettings Comm() => new();

    [Fact]
    public void Delta_D100_maps_to_0x1064()
    {
        var r = AddressResolver.Resolve("D100", Load("delta_dvp"), Comm());
        Assert.True(r.Ok, r.Error);
        Assert.Equal(ModbusTable.Holding, r.Ref.Table);
        Assert.Equal(0x1064, r.Ref.Address);
    }

    [Fact]
    public void Delta_high_ranges()
    {
        var p = Load("delta_dvp");
        Assert.Equal(0x9000 + (5000 - 4096), AddressResolver.Resolve("D5000", p, Comm()).Ref.Address);
        Assert.Equal(0xB000 + (2000 - 1280), AddressResolver.Resolve("M2000", p, Comm()).Ref.Address);
    }

    [Fact]
    public void Octal_X_and_Y()
    {
        var p = Load("delta_dvp");
        Assert.Equal(0x0400 + 8, AddressResolver.Resolve("X10", p, Comm()).Ref.Address);   // X10 bát phân = 8
        Assert.False(AddressResolver.Resolve("X18", p, Comm()).Ok);                         // chữ số 8 không hợp lệ
        Assert.Equal(ModbusTable.Discrete, AddressResolver.Resolve("X0", p, Comm()).Ref.Table);
    }

    [Fact]
    public void Bit_in_word()
    {
        var p = Load("delta_dvp");
        var r = AddressResolver.Resolve("D100.3", p, Comm());
        Assert.True(r.Ok); Assert.Equal(3, r.Ref.Bit);
        Assert.False(AddressResolver.Resolve("D100.16", p, Comm()).Ok);
        Assert.False(AddressResolver.Resolve("M10.1", p, Comm()).Ok);
    }

    [Fact]
    public void Siemens_uses_user_offset_and_stride()
    {
        var p = Load("siemens_s7_200smart");
        var c = Comm(); c.UserOffset = 1000;
        Assert.Equal(5, AddressResolver.Resolve("VW1010", p, c).Ref.Address);
        Assert.False(AddressResolver.Resolve("VW1011", p, c).Ok);   // lẻ
        Assert.False(AddressResolver.Resolve("VW998", p, c).Ok);    // dưới HoldStart
    }

    [Fact]
    public void Generic_styles()
    {
        var p = Load("generic");
        var c = Comm();
        Assert.Equal(100, AddressResolver.Resolve("HR100", p, c).Ref.Address);
        c.AddressStyle = AddressStyle.Base1;
        Assert.Equal(99, AddressResolver.Resolve("HR100", p, c).Ref.Address);
        Assert.False(AddressResolver.Resolve("HR0", p, c).Ok);
        c.AddressStyle = AddressStyle.Classic;
        var r = AddressResolver.Resolve("40101", p, c);
        Assert.Equal(ModbusTable.Holding, r.Ref.Table); Assert.Equal(100, r.Ref.Address);
        Assert.Equal(ModbusTable.Coil, AddressResolver.Resolve("00005", p, c).Ref.Table);
    }

    [Fact]
    public void Unknown_area_is_error()
        => Assert.False(AddressResolver.Resolve("Q5", Load("delta_dvp"), Comm()).Ok);

    [Fact]
    public void SevenSeg_rules()
    {
        Assert.True(SevenSeg.CanShow('S')); Assert.False(SevenSeg.CanShow('K'));
        Assert.Equal(4, SevenSeg.GlyphCount("12.34"));          // dấu chấm không chiếm ô
        var l = SevenSeg.Layout("2.5", 4);
        Assert.Equal(new byte[] { 0, 0, 0x5B | 0x80, 0x6D }, l);
        SevenSeg.Encode("KP", out var bad);
        Assert.Equal(new[] { 'K' }, bad);
    }

    static ProjectFile Sample()
    {
        var p = ProjectFile.CreateNew();
        p.Comm.ProfileId = "delta_dvp";
        p.Pages[0].Upper.Address = "D100"; p.Pages[0].Lower.Address = "D101";
        return p;
    }

    [Fact]
    public void Sample_compiles_with_valid_crc()
    {
        var res = RuntimeCompiler.Compile(Sample(), Load("delta_dvp"));
        Assert.True(res.Ok, string.Join("; ", res.Check.Issues.Where(i => i.Severity == Severity.Error).Select(i => i.Message)));
        var img = res.Image!;
        var dump = Environment.GetEnvironmentVariable("PKB_DUMP");
        if (!string.IsNullOrEmpty(dump)) File.WriteAllBytes(dump, img);
        Assert.Equal("PKRT", System.Text.Encoding.ASCII.GetString(img, 0, 4));
        Assert.Equal(1918, img.Length);
        Assert.Equal(1918, BitConverter.ToUInt16(img, 6));
        Assert.Equal(Crc32.Compute(img.AsSpan(0, 1914)), BitConverter.ToUInt32(img, 1914));
        Assert.Equal(0, img[12]);                    // comm.mode = master
        Assert.Equal(1, img[12 + 19]);               // comm.page_count
        Assert.Equal(1, img[42]);                    // page[0].top.type = hiện số
        Assert.Equal(2, img[42 + 24]);               // page[0].bot.type = nhập số
    }

    [Fact]
    public void Crc32_check_value() => Assert.Equal(0xCBF43926u, Crc32.Compute("123456789"u8));

    [Fact]
    public void Validator_catches_problems()
    {
        var prof = Load("delta_dvp");
        var p = Sample();
        p.Pages[0].Lower.Min = 10; p.Pages[0].Lower.Max = 5;            // min > max
        p.Pages.Add(new Page { Upper = new Cell { Type = CellType.Label, Label = "KEP" }, Lower = new Cell { Type = CellType.Input, Address = "D999999" } });
        var f1 = p.Keys.First(k => k.Id == "F1");
        f1.Action = KeyAction.GotoPage; f1.TargetPage = 9;
        p.Pages[0].Upper.Type = CellType.Input; p.Pages[0].Upper.DataType = DataType.Int32;   // D100..101 chồng D101
        var chk = Validator.Check(p, prof);
        var msgs = chk.Issues.Where(i => i.Severity == Severity.Error).Select(i => i.Message).ToList();
        Assert.Contains(msgs, m => m.Contains("Min"));
        Assert.Contains(msgs, m => m.Contains("không hiện được"));
        Assert.Contains(msgs, m => m.Contains("không tồn tại"));
        Assert.Contains(msgs, m => m.Contains("chồng lên"));
        Assert.Contains(msgs, m => m.Contains("D999999"));
    }

    [Fact]
    public void Too_many_digits_for_row_is_error()
    {
        var p = Sample(); p.Pages[0].Lower.Max = 99999;
        Assert.Contains(Validator.Check(p, Load("delta_dvp")).Issues, i => i.Severity == Severity.Error && i.Message.Contains("ô"));
        p.Pages[0].Lower.Span8 = true;   // Span8 chỉ áp dụng hàng trên: vẫn lỗi
        Assert.True(Validator.Check(p, Load("delta_dvp")).HasErrors);
        p.Pages[0].Upper.Span8 = true; p.Pages[0].Lower.Type = CellType.None;
        Assert.False(Validator.Check(p, Load("delta_dvp")).HasErrors);
    }

    [Fact]
    public void Input_to_readonly_area_is_error()
    {
        var p = ProjectFile.CreateNew(); p.Comm.ProfileId = "generic";
        p.Pages[0].Lower.Address = "IR5";
        Assert.Contains(Validator.Check(p, Load("generic")).Issues, i => i.Severity == Severity.Error && i.Message.Contains("Không ghi được"));
    }

    [Fact]
    public void Download_to_simulated_device()
    {
        var res = RuntimeCompiler.Compile(Sample(), Load("delta_dvp"));
        var dev = new SimulatedDevice();
        var cli = new ConfigClient(dev);
        var info = cli.ReadInfo();
        Assert.Equal(RuntimeCompiler.FormatVersion, info.StructVersion);
        int last = 0;
        cli.Download(res.Image!, new SyncProgress(v => last = v));
        Assert.Equal(res.Image, dev.AppliedImage);
        Assert.Equal(100, last);
        cli.Command(ConfigCommand.Identify);
        Assert.Equal(1, dev.IdentifyCount);
    }

    [Fact]
    public void Download_retries_after_timeout_but_detects_corruption()
    {
        var res = RuntimeCompiler.Compile(Sample(), Load("delta_dvp"));
        var dev = new SimulatedDevice { DropNext = 1 };
        new ConfigClient(dev).Download(res.Image!);
        Assert.NotNull(dev.AppliedImage);

        var bad = new SimulatedDevice { CorruptWrites = true };
        Assert.Throws<ConfigException>(() => new ConfigClient(bad).Download(res.Image!));
        Assert.Null(bad.AppliedImage);
    }

    [Fact]
    public void Image_of_wrong_size_is_rejected()
    {
        var dev = new SimulatedDevice();
        Assert.Throws<ConfigException>(() => new ConfigClient(dev).Download(new byte[100]));
    }

    [Fact]
    public void Project_roundtrip()
    {
        var p = Sample(); p.Pages[0].Upper.Span8 = true; p.Leds[0].Mode = LedMode.Blink; p.Leds[0].Address = "M5";
        var f = Path.GetTempFileName();
        try
        {
            ProjectStorage.Save(p, f);
            var q = ProjectStorage.Load(f);
            Assert.Equal(p.Pages[0].Upper.Address, q.Pages[0].Upper.Address);
            Assert.True(q.Pages[0].Upper.Span8); Assert.True(q.Pages[0].Upper.IsUpper); Assert.False(q.Pages[0].Lower.IsUpper);
            Assert.Equal(LedMode.Blink, q.Leds[0].Mode);
            Assert.Equal(5, q.Keys.Count); Assert.Equal(8, q.Leds.Count);
        }
        finally { File.Delete(f); }
    }

    [Fact]
    public void Slave_mode_has_no_objects()
    {
        var p = Sample(); p.Comm.Mode = OperationMode.Slave;
        var res = RuntimeCompiler.Compile(p, Load("delta_dvp"));
        Assert.True(res.Ok);
        Assert.Equal(1, res.Image![12]);                // comm.mode = slave
        Assert.Equal(0, res.Image![12 + 19]);           // không có trang
    }

    sealed class SyncProgress : IProgress<int>
    {
        readonly Action<int> _a;
        public SyncProgress(Action<int> a) => _a = a;
        public void Report(int v) => _a(v);
    }
}

public class MitsubishiProfileTests
{
    static PlcProfile Load(string id) => PlcProfile.Load(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "profiles", id + ".json"));
    static ModbusRef R(PlcProfile p, string a) => AddressResolver.Resolve(a, p, new CommSettings()).Ref;

    [Fact]
    public void Fx3u_addresses_match_manual_table()
    {
        var p = Load("mitsubishi_fx");
        Assert.Equal(0x0000, R(p, "M0").Address);
        Assert.Equal(0x1DFF, R(p, "M7679").Address);
        Assert.Equal(0x1E00, R(p, "M8000").Address);
        Assert.Equal(0x2000, R(p, "S0").Address);
        Assert.Equal(0x3300, R(p, "Y0").Address);
        Assert.Equal(0x3300 + 8, R(p, "Y10").Address);          // bát phân
        Assert.Equal(0x3400, R(p, "X0").Address);
        Assert.Equal(ModbusTable.Discrete, R(p, "X0").Table);
        Assert.Equal(0x1F40, R(p, "D8000").Address);
        Assert.Equal(0x2140, R(p, "R0").Address);
        Assert.Equal(0xA140, R(p, "TN0").Address);
        Assert.Equal(0xA340, R(p, "CN0").Address);
        Assert.False(AddressResolver.Resolve("Y8", p, new CommSettings()).Ok);   // chữ số 8 không hợp lệ ở cơ số 8
    }

    [Fact]
    public void Fx3g_addresses_match_manual_table()
    {
        var p = Load("mitsubishi_fx3g");
        Assert.Equal(0x3140, R(p, "CS0").Address);
        Assert.Equal(0x3200, R(p, "Y0").Address);
        Assert.Equal(0x3300, R(p, "X0").Address);
        Assert.Equal(0x2140, R(p, "R0").Address);
    }
}

public class AbsoluteAddressTests
{
    static PlcProfile Load(string id) => PlcProfile.Load(Path.Combine(AppContext.BaseDirectory, "..", "..", "..", "..", "..", "profiles", id + ".json"));

    [Fact]
    public void Short_to_absolute_and_back_for_fx3u()
    {
        AddressContext.Profile = Load("mitsubishi_fx"); AddressContext.Comm = new CommSettings();
        Assert.Equal("HR:100", AddressContext.ToAbsolute("D100"));
        Assert.Equal("C:13064", AddressContext.ToAbsolute("Y10"));       // 0x3300 + 8 (bát phân)
        Assert.Equal("HR:100.3", AddressContext.ToAbsolute("D100.3"));
        Assert.Equal("D100", AddressContext.FromAbsolute("HR:100"));
        Assert.Equal("D8000", AddressContext.FromAbsolute("HR:0x1F40"));
        Assert.Equal("Y10", AddressContext.FromAbsolute("C:13064"));
        Assert.Equal("D100.3", AddressContext.FromAbsolute("HR:100.3"));
        Assert.Equal("X20", AddressContext.FromAbsolute("DI:" + (13312 + 16)));
    }

    [Fact]
    public void Absolute_without_matching_area_is_kept_and_resolves_everywhere()
    {
        var fx = Load("mitsubishi_fx");
        AddressContext.Profile = fx; AddressContext.Comm = new CommSettings();
        Assert.Equal("C:60000", AddressContext.FromAbsolute("c:60000"));    // không thuộc vùng nào của FX3U
        var r = AddressResolver.Resolve("C:60000", fx, new CommSettings());
        Assert.True(r.Ok); Assert.Equal(60000, r.Ref.Address); Assert.Equal(ModbusTable.Coil, r.Ref.Table);
        Assert.False(AddressResolver.Resolve("C:70000", fx, new CommSettings()).Ok);
        Assert.False(AddressResolver.Resolve("C:5.1", fx, new CommSettings()).Ok);
    }

    [Fact]
    public void Generic_profile_respects_numbering_style()
    {
        var g = Load("generic");
        var c = new CommSettings { AddressStyle = AddressStyle.Base1 };
        AddressContext.Profile = g; AddressContext.Comm = c;
        Assert.Equal("HR:99", AddressContext.ToAbsolute("HR100"));       // kiểu đánh số từ 1
        Assert.Equal("HR100", AddressContext.FromAbsolute("HR:99"));
    }
}
