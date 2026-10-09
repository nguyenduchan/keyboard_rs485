namespace PanelKbd.Core;

/// <summary>
/// Thử phần cứng bàn phím thật qua thanh ghi slave (docs/CONFIG_PROTOCOL.md §2.5): bitmap phím 5–6,
/// bộ đếm 1, hai hàng số 10–13, LED 14, còi 15. Dùng được cả khi bàn phím ở chế độ cấu hình (trạm 247)
/// lẫn khi chạy slave (trạm riêng). PC làm master, không cần PLC.
/// </summary>
public sealed class HardwareProbe
{
    public const ushort RegKeys = ConfigClient.RegKeysLo, RegCounter = ConfigClient.RegKeyCount, RegLeds = ConfigClient.RegLed;

    /// <summary>Tên phím theo bit của bitmap (bit n = SW(n+1)). SW1…SW4 là hàng dưới cùng, trái → phải (sơ đồ nguyên lý, mục ma trận phím).</summary>
    public static readonly string[] KeyByBit =
        { "F1", "0", "F2", "ENT", "1", "2", "3", "DN", "4", "5", "6", "UP", "7", "8", "9", "ESC" };

    readonly ConfigClient _c;
    public int TimeoutMs { get => _c.TimeoutMs; set => _c.TimeoutMs = value; }
    public int Retries { get => _c.Retries; set => _c.Retries = value; }
    public event Action<bool, byte[]>? Frame { add => _c.Frame += value; remove => _c.Frame -= value; }

    public HardwareProbe(IModbusPort port, byte station) => _c = new ConfigClient(port) { Station = station };

    public (ushort Keys, ushort Counter, ushort Row1, ushort Row2, ushort Leds) Read()
    {
        var l = _c.ReadLive();
        return ((ushort)l.KeysDown, l.KeyCount, (ushort)Math.Clamp(l.Top, 0, ushort.MaxValue), (ushort)Math.Clamp(l.Bottom, 0, ushort.MaxValue), l.Led);
    }

    public void Write(ushort addr, ushort value) => _c.WriteRegister(addr, value);
    public void WriteDisplay(ushort row1, ushort row2) => _c.WriteDisplay(row1, row2);
    public void Beep(bool longBeep = false) => _c.Beep(longBeep);

    public static IEnumerable<string> PressedKeys(ushort mask)
    {
        for (int i = 0; i < 16; i++) if ((mask & (1 << i)) != 0) yield return KeyByBit[i];
    }
}
