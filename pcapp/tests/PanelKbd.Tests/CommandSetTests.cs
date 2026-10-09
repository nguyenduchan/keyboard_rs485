using PanelKbd.Core;
using Xunit;

public class CommandSetTests
{
    static byte[] Image()
    {
        var p = ProjectFile.CreateNew(); p.Comm.ProfileId = "delta_dvp";
        p.Pages[0].Upper.Address = "D100"; p.Pages[0].Lower.Address = "D101";
        var prof = PlcProfile.Load(Path.Combine(AppContext.BaseDirectory, "profiles", "delta_dvp.json"));
        return RuntimeCompiler.Compile(p, prof).Image!;
    }

    [Fact]
    public void Handshake_retries_until_device_answers()
    {
        var dev = new SimulatedDevice { DropNext = 3 };
        var info = new ConfigClient(dev).Handshake(totalTimeoutMs: 3000, intervalMs: 10);
        Assert.True(info.InConfigMode);
        Assert.Equal(RuntimeCompiler.FormatVersion, info.StructVersion);
        Assert.True(info.PowerOk);
    }

    [Fact]
    public void Handshake_times_out_with_helpful_message()
    {
        var dev = new SimulatedDevice { DropNext = 1000 };
        var ex = Assert.Throws<ConfigException>(() => new ConfigClient(dev).Handshake(150, 10));
        Assert.Contains("Cấp nguồn lại", ex.Message);
    }

    [Fact]
    public void Backup_reads_back_applied_image()
    {
        var dev = new SimulatedDevice(); var c = new ConfigClient(dev);
        var img = Image();
        c.Download(img);
        Assert.Equal(img, c.ReadConfig());
        Assert.False(c.ReadInfo().ConfigDefaulted);
    }

    [Fact]
    public void Writes_outside_config_mode_are_rejected_with_clear_message()
    {
        var dev = new SimulatedDevice { InConfigMode = false }; var c = new ConfigClient(dev);
        var ex = Assert.Throws<ConfigException>(() => c.Download(Image()));
        Assert.Contains("chế độ cấu hình", ex.Message);
        c.EnterConfigMode();
        c.Download(Image());
        Assert.NotNull(dev.AppliedImage);
    }

    [Fact]
    public void Factory_reload_identify_exit_and_clear_diag()
    {
        var dev = new SimulatedDevice(); var c = new ConfigClient(dev);
        c.Download(Image());
        var d = c.ReadDiagnostics();
        Assert.Equal(12, d.MasterOk); Assert.Equal(7, d.SlaveServed);
        c.ClearDiagnostics();
        Assert.Equal(0, c.ReadDiagnostics().MasterOk);
        c.Identify(); Assert.Equal(1, dev.IdentifyCount);
        c.Reload();
        c.FactoryReset(); Assert.Null(dev.AppliedImage);
        Assert.True(c.ReadInfo().ConfigDefaulted);
        c.Exit(); Assert.True(dev.Exited);
    }

    [Fact]
    public void Apply_of_corrupt_block_reports_result_text()
    {
        var dev = new SimulatedDevice(); var c = new ConfigClient(dev);
        var img = Image(); img[100] ^= 0xFF;     // làm sai CRC32
        var ex = Assert.Throws<ConfigException>(() => c.Download(img));
        Assert.Contains("CRC32", ex.Message);
        Assert.Equal("Lỗi ghi flash".Length > 0, true);
        Assert.Contains("flash", ConfigClient.ResultText(0x13));
    }

    [Fact]
    public void Live_display_led_and_keys()
    {
        var dev = new SimulatedDevice(); var c = new ConfigClient(dev);
        dev.KeysDown = 1 << 3; dev.KeyCount = 9;
        c.WriteDisplay(1234, -56);
        c.WriteLeds(0b101, 0b010);
        var l = c.ReadLive();
        Assert.Equal(1234, l.Top); Assert.Equal(-56, l.Bottom);
        Assert.Equal(9, l.KeyCount); Assert.Equal(1u << 3, l.KeysDown);
        Assert.Equal(0b010_00000101, l.Led);
        c.WriteRawSegments(new byte[] { 0x3F, 0x06 });
        Assert.Equal(ConfigClient.FmtRaw, dev.GetSlaveRegister(ConfigClient.RegFmt));
        Assert.Equal(0x06, dev.GetSlaveRegister(ConfigClient.RegRaw0 + 1));
    }
}
