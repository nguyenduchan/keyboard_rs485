using PanelKbd.Core;
using Xunit;

public class SimTests
{
    static PlcProfile Delta() => PlcProfile.Load(Path.Combine(AppContext.BaseDirectory, "profiles", "delta_dvp.json"));

    static (ProjectFile p, PlcMemory m, KeyboardEmulator e) Make()
    {
        var p = ProjectFile.CreateNew();
        p.Comm.ProfileId = "delta_dvp"; p.Comm.WordOrder = WordOrder.CDAB;
        p.Pages[0].Upper = new Cell { Type = CellType.Display, Address = "D100", Decimals = 1 };
        p.Pages[0].Lower = new Cell { Type = CellType.Input, Address = "D101", Decimals = 1, Min = 0, Max = 9999 };
        var m = new PlcMemory();
        return (p, m, new KeyboardEmulator(p, Delta(), m));
    }

    static void Type(KeyboardEmulator e, string keys) { foreach (var k in keys.Split(' ')) { e.Press(k); e.Release(k); } }

    [Fact]
    public void Display_reads_plc_value_with_decimals()
    {
        var (_, m, e) = Make();
        m.SetWord(ModbusTable.Holding, 0x1064, 1234);
        e.Refresh();
        Assert.Equal(SevenSeg.Layout("123.4", 4), e.Segments[..4]);
    }

    [Fact]
    public void Typing_digits_and_enter_writes_value()
    {
        var (_, m, e) = Make();
        Type(e, "2 5 0 ENT");
        Assert.Equal(250, m.GetWord(ModbusTable.Holding, 0x1065));
    }

    [Fact]
    public void Out_of_range_is_rejected_and_nothing_written()
    {
        var (p, m, e) = Make();
        p.Pages[0].Lower.Max = 100;
        Type(e, "5 0 0 ENT");
        Assert.Equal(0, m.GetWord(ModbusTable.Holding, 0x1065));
        Assert.Equal(SevenSeg.Layout("E-Hi", 4), e.Segments[..4]);
    }

    [Fact]
    public void Esc_deletes_digit_and_cancels()
    {
        var (_, m, e) = Make();
        Type(e, "1 2 ESC 3 ENT");
        Assert.Equal(13, m.GetWord(ModbusTable.Holding, 0x1065));
    }

    [Fact]
    public void Password_blocks_until_correct()
    {
        var (p, m, e) = Make();
        p.Pages[0].Lower.PasswordLevel = 1; p.Security.OperatorPassword = "12";
        Type(e, "9 ENT");                       // sai
        Assert.Equal(0, e.Level);
        Type(e, "1 2 ENT");                     // đúng
        Assert.Equal(1, e.Level);
        Type(e, "7 ENT");
        Assert.Equal(7, m.GetWord(ModbusTable.Holding, 0x1065));
    }

    [Fact]
    public void Function_keys_and_leds()
    {
        var (p, m, e) = Make();
        var f1 = p.Keys.First(k => k.Id == "F1"); f1.Action = KeyAction.Hold; f1.Address = "M10";
        var f2 = p.Keys.First(k => k.Id == "F2"); f2.Action = KeyAction.Toggle; f2.Address = "M11";
        var led = p.Leds[0]; led.Mode = LedMode.Steady; led.Address = "M11";
        e.Press("F1");
        Assert.True(m.GetBit(ModbusTable.Coil, 0x0800 + 10));
        e.Release("F1");
        Assert.False(m.GetBit(ModbusTable.Coil, 0x0800 + 10));
        Type(e, "F2");
        Assert.True(m.GetBit(ModbusTable.Coil, 0x0800 + 11));
        Assert.True(e.LedLit[0]);
    }

    [Fact]
    public void Page_navigation_and_missing_key()
    {
        var (p, _, e) = Make();
        p.Pages.Add(new Page { Upper = new Cell { Type = CellType.Label, Label = "SEt" } });
        Type(e, "DN");
        Assert.Equal(1, e.PageIndex);
        p.Keys.First(k => k.Id == "UP").Present = false;
        Type(e, "UP");
        Assert.Equal(1, e.PageIndex);           // phím không lắp → bỏ qua
        Type(e, "ESC");
        Assert.Equal(0, e.PageIndex);           // ESC về trang RUN
    }

    [Fact]
    public void Int32_respects_word_order()
    {
        var (p, m, e) = Make();
        var u = p.Pages[0].Upper;
        u.Type = CellType.Input; u.DataType = DataType.Int32; u.Span8 = true; u.Decimals = 0; u.Max = 99999999;
        Type(e, "7 0 0 0 0 ENT");
        Assert.Equal(70000 & 0xFFFF, m.GetWord(ModbusTable.Holding, 0x1064));
        Assert.Equal(70000 >> 16, m.GetWord(ModbusTable.Holding, 0x1065));
    }

    [Fact]
    public void Slave_engine_roundtrip_and_exceptions()
    {
        var m = new PlcMemory(); var eng = new ModbusSlaveEngine(m) { Station = 3 };
        Assert.NotNull(eng.Handle(ModbusRtu.WriteRegs(3, 10, new ushort[] { 11, 22 })));
        Assert.Equal(22, m.GetWord(ModbusTable.Holding, 11));
        var r = eng.Handle(ModbusRtu.ReadRegs(3, 3, 10, 2))!;
        Assert.Equal(11, (r[3] << 8) | r[4]);
        Assert.Null(eng.Handle(ModbusRtu.ReadRegs(4, 3, 10, 2)));                        // trạm khác
        Assert.Equal(0x83, eng.Handle(ModbusRtu.ReadRegs(3, 3, 65535, 5))![1]);           // ngoài bảng
        eng.Handle(ModbusRtu.WithCrc(new byte[] { 3, 5, 0, 7, 0xFF, 0 }));
        Assert.True(m.GetBit(ModbusTable.Coil, 7));
    }

    [Fact]
    public void Hardware_probe_on_virtual_firmware()
    {
        var m = new PlcMemory(); var eng = new ModbusSlaveEngine(m) { Station = 1 };
        var probe = new HardwareProbe(new EnginePort(eng), 1);
        m.SetWord(ModbusTable.Holding, 5, 1 << 3 | 1 << 12); m.SetWord(ModbusTable.Holding, 1, 5);
        var r = probe.Read();
        Assert.Equal(new[] { "ENT", "7" }, HardwareProbe.PressedKeys(r.Keys).ToArray());
        Assert.Equal(5, r.Counter);
        probe.WriteDisplay(1234, 5678);
        Assert.Equal(5678, m.GetWord(ModbusTable.Holding, 13));
        Assert.Equal(1234, m.GetWord(ModbusTable.Holding, 11));
    }

    [Fact]
    public void Layout_hidden_rows_are_skipped_by_validator_and_compiler()
    {
        var p = ProjectFile.CreateNew(); p.Comm.ProfileId = "delta_dvp";
        p.Pages[0].Upper.Address = "D100"; p.Pages[0].Lower.Address = "ZZZ";    // địa chỉ hỏng ở hàng sẽ bị ẩn
        Assert.True(Validator.Check(p, Delta()).HasErrors);
        p.Layout.HasLower = false;
        Assert.False(Validator.Check(p, Delta()).HasErrors);
        var img = RuntimeCompiler.Compile(p, Delta()).Image!;
        Assert.Equal(1, img[15]);
    }
}

public class PlcFaultTests
{
    [Fact]
    public void Silent_and_forced_exception_and_bad_length()
    {
        var e = new ModbusSlaveEngine(new PlcMemory()) { Station = 1 };
        var rd = ModbusRtu.ReadRegs(1, 3, 0, 1);
        Assert.NotNull(e.Handle(rd));
        e.ForceException = 2;
        var r = e.Handle(rd)!;
        Assert.Equal(0x83, r[1]); Assert.Equal(2, r[2]);
        e.ForceException = 0; e.Silent = true;
        Assert.Null(e.Handle(rd));
        e.Silent = false;
        var bad = ModbusRtu.WithCrc(new byte[] { 1, 5, 0, 0, 0xFF, 0, 9 });   // FC05 dài sai
        Assert.Null(e.Handle(bad));
    }

    [Fact]
    public void Expected_length_from_header()
    {
        Assert.Equal(8, ModbusSlaveEngine.ExpectedRequestLength(new byte[] { 1, 3 }));
        Assert.Equal(-1, ModbusSlaveEngine.ExpectedRequestLength(new byte[] { 1, 16, 0, 0 }));
        Assert.Equal(9 + 4, ModbusSlaveEngine.ExpectedRequestLength(new byte[] { 1, 16, 0, 0, 0, 2, 4 }));
    }
}
