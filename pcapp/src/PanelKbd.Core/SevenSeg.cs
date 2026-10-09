namespace PanelKbd.Core;

/// <summary>Bảng mã 7 đoạn. Bit 0..6 = a..g, bit 7 = dấu chấm.</summary>
public static class SevenSeg
{
    public const byte Dp = 0x80;

    static readonly Dictionary<char, byte> Font = new()
    {
        ['0'] = 0x3F, ['1'] = 0x06, ['2'] = 0x5B, ['3'] = 0x4F, ['4'] = 0x66,
        ['5'] = 0x6D, ['6'] = 0x7D, ['7'] = 0x07, ['8'] = 0x7F, ['9'] = 0x6F,
        ['A'] = 0x77, ['b'] = 0x7C, ['C'] = 0x39, ['c'] = 0x58, ['d'] = 0x5E,
        ['E'] = 0x79, ['F'] = 0x71, ['G'] = 0x3D, ['H'] = 0x76, ['h'] = 0x74,
        ['I'] = 0x30, ['J'] = 0x1E, ['L'] = 0x38, ['n'] = 0x54, ['N'] = 0x37,
        ['o'] = 0x5C, ['O'] = 0x3F, ['P'] = 0x73, ['q'] = 0x67, ['r'] = 0x50,
        ['S'] = 0x6D, ['t'] = 0x78, ['U'] = 0x3E, ['u'] = 0x1C, ['y'] = 0x6E,
        ['-'] = 0x40, ['_'] = 0x08, [' '] = 0x00, ['°'] = 0x63,
    };

    public static bool CanShow(char c) => Font.ContainsKey(c);

    public static string Supported => "0-9 A b C c d E F G H h I J L n N o O P q r S t U u y - _ ° và dấu cách";

    /// <summary>
    /// Mã hoá chuỗi thành các ký tự 7 đoạn (dấu '.' gắn vào ký tự đứng trước).
    /// Ký tự không hiện được trả về trong <paramref name="bad"/> (và hiển thị trống).
    /// </summary>
    public static byte[] Encode(string text, out List<char> bad)
    {
        bad = new List<char>();
        var res = new List<byte>();
        foreach (char c in text)
        {
            if (c == '.')
            {
                if (res.Count > 0 && (res[^1] & Dp) == 0) res[^1] |= Dp;
                else res.Add(Dp);
                continue;
            }
            if (Font.TryGetValue(c, out var m)) res.Add(m);
            else { bad.Add(c); res.Add(0); }
        }
        return res.ToArray();
    }

    /// <summary>Số ký tự chiếm chỗ (dấu chấm gắn vào ký tự trước không tính).</summary>
    public static int GlyphCount(string text) => Encode(text, out _).Length;

    /// <summary>Căn phải vào <paramref name="digits"/> ô, thiếu thì đệm trống phía trái.</summary>
    public static byte[] Layout(string text, int digits)
    {
        var g = Encode(text, out _);
        var res = new byte[digits];
        int n = Math.Min(g.Length, digits);
        Array.Copy(g, g.Length - n, res, digits - n, n);
        return res;
    }

    /// <summary>Nhãn tĩnh: căn trái, tối đa 4 ký tự, bỏ dấu chấm.</summary>
    public static byte[] LabelBytes(string text)
    {
        var g = Encode(text, out _);
        var res = new byte[4];
        for (int i = 0; i < Math.Min(4, g.Length); i++) res[i] = (byte)(g[i] & 0x7F);
        return res;
    }
}
