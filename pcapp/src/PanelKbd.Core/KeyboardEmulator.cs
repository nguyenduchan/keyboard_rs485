namespace PanelKbd.Core;

/// <summary>
/// Mô phỏng hành vi bàn phím theo dự án đã thiết kế (cùng quy tắc nhập liệu của firmware):
/// đọc giá trị từ PlcMemory để hiện, ghi vào PlcMemory khi ENT / phím chức năng.
/// Dùng để thử thiết kế khi không có PLC lẫn bàn phím.
/// </summary>
public sealed class KeyboardEmulator
{
    readonly ProjectFile _p;
    readonly PlcProfile _prof;
    readonly PlcMemory _mem;

    public int PageIndex { get; private set; }
    public byte[] Segments { get; } = new byte[8];
    public bool[] LedLit { get; } = new bool[8];
    public string Status { get; private set; } = "";
    public string LastWrite { get; private set; } = "";
    public int Level { get; private set; }                 // 0 chưa đăng nhập, 1 vận hành, 2 kỹ thuật
    public event Action? Changed;
    public event Action<string>? Event;                     // nhật ký thao tác

    Cell? _editCell; string _buf = ""; bool _neg;
    int _promptLevel;                                       // >0: đang nhập mật khẩu
    int _idleMs, _loginMs, _msgMs, _blinkMs; bool _blink;
    string _msg = "";
    readonly HashSet<string> _held = new();

    public KeyboardEmulator(ProjectFile p, PlcProfile prof, PlcMemory mem)
    {
        _p = p; _prof = prof; _mem = mem;
        PageIndex = Math.Clamp(p.Security.RunPage - 1, 0, Math.Max(0, p.Pages.Count - 1));
        Refresh();
    }

    public Page? CurrentPage => PageIndex < _p.Pages.Count ? _p.Pages[PageIndex] : null;

    // ---------- Truy cập bộ nhớ ----------
    ResolveResult Res(string addr) => AddressResolver.Resolve(addr, _prof, _p.Comm);

    bool TryReadValue(Cell c, out long value)
    {
        value = 0;
        var r = Res(c.Address); if (!r.Ok || r.Ref.Table.IsBitTable()) return false;
        var m = r.Ref;
        ushort w0 = _mem.GetWord(m.Table, m.Address);
        if (m.HasBit) { value = (w0 >> m.Bit) & 1; return true; }
        if (c.DataType.Registers() == 1)
        {
            value = c.DataType == DataType.Int16 ? (short)w0 : w0; return true;
        }
        ushort w1 = _mem.GetWord(m.Table, m.Address + 1);
        uint u = _p.Comm.WordOrder == WordOrder.CDAB ? ((uint)w1 << 16) | w0 : ((uint)w0 << 16) | w1;
        value = c.DataType == DataType.Int32 ? (int)u : u;
        return true;
    }

    bool TryWriteValue(Cell c, long v)
    {
        var r = Res(c.Address); if (!r.Ok || r.Ref.Table != ModbusTable.Holding) return false;
        var m = r.Ref;
        if (m.HasBit) return false;
        if (c.DataType.Registers() == 1) _mem.SetWord(m.Table, m.Address, (ushort)(short)v);
        else
        {
            uint u = (uint)v;
            ushort hi = (ushort)(u >> 16), lo = (ushort)u;
            if (_p.Comm.WordOrder == WordOrder.CDAB) { _mem.SetWord(m.Table, m.Address, lo); _mem.SetWord(m.Table, m.Address + 1, hi); }
            else { _mem.SetWord(m.Table, m.Address, hi); _mem.SetWord(m.Table, m.Address + 1, lo); }
        }
        LastWrite = $"{c.Address} ← {v}";
        return true;
    }

    bool ReadBit(string addr)
    {
        var r = Res(addr); if (!r.Ok) return false;
        var m = r.Ref;
        if (m.Table.IsBitTable()) return _mem.GetBit(m.Table, m.Address);
        return m.HasBit && ((_mem.GetWord(m.Table, m.Address) >> m.Bit) & 1) != 0;
    }

    bool WriteBit(string addr, bool v)
    {
        var r = Res(addr); if (!r.Ok || !r.Ref.Table.IsWritable()) return false;
        var m = r.Ref;
        if (m.Table.IsBitTable()) _mem.SetBit(m.Table, m.Address, v);
        else if (m.HasBit)
        {
            ushort w = _mem.GetWord(m.Table, m.Address);
            w = v ? (ushort)(w | (1 << m.Bit)) : (ushort)(w & ~(1 << m.Bit));
            _mem.SetWord(m.Table, m.Address, w);
        }
        else return false;
        LastWrite = $"{addr} ← {(v ? 1 : 0)}";
        return true;
    }

    // ---------- Phím ----------
    public static readonly string[] DigitIds = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };

    public void Press(string id)
    {
        bool present = id.Length > 0 && (_p.Layout.IsPresent(id) || _p.Keys.Any(k => k.Id == id && k.Present));
        var cfg = _p.Keys.FirstOrDefault(k => k.Id == id);
        if (cfg != null) present = cfg.Present;
        if (!present) { Event?.Invoke($"Phím {id}: board không lắp — bỏ qua"); return; }
        if (!_held.Add(id)) return;                          // chống lặp khi giữ phím
        _idleMs = 0;
        Event?.Invoke($"Nhấn {id}");

        if (cfg != null) { DoConfigKey(cfg, true); Changed?.Invoke(); return; }
        if (id.Length == 1 && char.IsDigit(id[0])) Digit(id[0]);
        else if (id == "ESC") Esc();
        else if (id == "ENT") Enter();
        Refresh(); Changed?.Invoke();
    }

    public void Release(string id)
    {
        if (!_held.Remove(id)) return;
        var cfg = _p.Keys.FirstOrDefault(k => k.Id == id);
        if (cfg != null && cfg.Present && cfg.Action == KeyAction.Hold && WriteBit(cfg.Address, false)) Event?.Invoke($"Nhả {id}: {LastWrite}");
        Refresh(); Changed?.Invoke();
    }

    void DoConfigKey(KeyConfig k, bool press)
    {
        CancelEdit();
        switch (k.Action)
        {
            case KeyAction.Hold: case KeyAction.Set:
                if (WriteBit(k.Address, true)) Event?.Invoke($"{k.Title}: {LastWrite}"); else Flash("E-Ad"); break;
            case KeyAction.Reset:
                if (WriteBit(k.Address, false)) Event?.Invoke($"{k.Title}: {LastWrite}"); else Flash("E-Ad"); break;
            case KeyAction.Toggle:
                if (WriteBit(k.Address, !ReadBit(k.Address))) Event?.Invoke($"{k.Title}: {LastWrite}"); else Flash("E-Ad"); break;
            case KeyAction.WriteConst:
            {
                var r = Res(k.Address);
                if (r.Ok && r.Ref.Table == ModbusTable.Holding && !r.Ref.Table.IsBitTable())
                {
                    _mem.SetWord(r.Ref.Table, r.Ref.Address, (ushort)k.Const);
                    LastWrite = $"{k.Address} ← {k.Const}"; Event?.Invoke($"{k.Title}: {LastWrite}");
                }
                else Flash("E-Ad");
                break;
            }
            case KeyAction.GotoPage: GoPage(k.TargetPage - 1); break;
            case KeyAction.PrevPage: GoPage(PageIndex - 1); break;
            case KeyAction.NextPage: GoPage(PageIndex + 1); break;
        }
    }

    void GoPage(int i)
    {
        if (i < 0 || i >= _p.Pages.Count) return;
        PageIndex = i;
        Event?.Invoke($"Trang {i + 1}");
    }

    Cell? EditTarget()
    {
        var pg = CurrentPage; if (pg == null) return null;
        if (_p.Layout.HasLower && !pg.Upper.Span8 && pg.Lower.Type == CellType.Input) return pg.Lower;
        if (_p.Layout.HasUpper && pg.Upper.Type == CellType.Input) return pg.Upper;
        return null;
    }

    void Digit(char d)
    {
        if (_promptLevel > 0) { if (_buf.Length < 4) _buf += d; return; }
        if (_editCell == null)
        {
            var t = EditTarget(); if (t == null) return;
            if (t.PasswordLevel > Level)
            {
                _promptLevel = t.PasswordLevel; _buf = "";
                if (_buf.Length < 4) _buf += d;
                return;
            }
            _editCell = t; _buf = ""; _neg = false;
        }
        if (_buf.Length < _editCell.DigitCount && !(_buf == "0" && d == '0')) _buf = (_buf == "0" ? "" : _buf) + d;
    }

    void Esc()
    {
        if (_promptLevel > 0) { if (_buf.Length > 0) _buf = _buf[..^1]; else { _promptLevel = 0; } return; }
        if (_editCell != null) { if (_buf.Length > 0) _buf = _buf[..^1]; else CancelEdit(); return; }
        GoPage(_p.Security.RunPage - 1);
    }

    void CancelEdit() { _editCell = null; _buf = ""; _neg = false; _promptLevel = 0; }

    void Enter()
    {
        if (_promptLevel > 0)
        {
            int need = _promptLevel; string got = _buf;
            CancelEdit();
            string op = _p.Security.OperatorPassword, tech = _p.Security.TechPassword;
            int lvl = tech.Length > 0 && got == tech ? 2 : op.Length > 0 && got == op ? 1 : 0;
            if (lvl >= need)
            {
                Level = Math.Max(Level, lvl); _loginMs = 0; Flash("LoG"); Event?.Invoke($"Đăng nhập cấp {Level}");
            }
            else { Flash("E-Pd"); Event?.Invoke("Sai mật khẩu"); }
            return;
        }
        if (_editCell == null) return;
        var c = _editCell;
        long v = _buf.Length == 0 ? 0 : long.Parse(_buf);
        if (_neg) v = -v;
        if (v > c.Max) { Flash("E-Hi"); Event?.Invoke($"{v} > Max {c.Max}"); CancelEdit(); return; }
        if (v < c.Min) { Flash("E-Lo"); Event?.Invoke($"{v} < Min {c.Min}"); CancelEdit(); return; }
        if (TryWriteValue(c, v)) { Flash("donE"); Event?.Invoke("Ghi " + LastWrite); }
        else { Flash("E-Ad"); Event?.Invoke("Không ghi được " + c.Address); }
        CancelEdit();
    }

    void Flash(string text) { _msg = text; _msgMs = 800; }

    // ---------- Chu kỳ 10 Hz ----------
    public void Tick(int ms)
    {
        _idleMs += ms; _blinkMs += ms;
        if (_blinkMs >= 500) { _blinkMs = 0; _blink = !_blink; }
        if (_msgMs > 0) { _msgMs -= ms; if (_msgMs <= 0) _msg = ""; }
        if (Level > 0 && _p.Security.AutoLogoutSec > 0) { _loginMs += ms; if (_loginMs >= _p.Security.AutoLogoutSec * 1000) { Level = 0; Event?.Invoke("Tự đăng xuất"); } }
        else if (Level > 0) _loginMs = 0;
        if (_p.Security.RunTimeoutSec > 0 && _idleMs >= _p.Security.RunTimeoutSec * 1000 && PageIndex != _p.Security.RunPage - 1)
        { CancelEdit(); GoPage(_p.Security.RunPage - 1); }
        Refresh(); Changed?.Invoke();
    }

    // ---------- Hiển thị ----------
    string FormatCell(Cell c, int digits, out bool err)
    {
        err = false;
        switch (c.Type)
        {
            case CellType.Label: return c.Label;
            case CellType.BitState:
                var r = Res(c.Address); if (!r.Ok) { err = true; return "E-Ad"; }
                return ReadBit(c.Address) ? c.OnLabel : c.OffLabel;
            case CellType.Display:
            case CellType.Input:
                if (ReferenceEquals(c, _editCell))
                {
                    string d = _buf.Length == 0 ? "0" : _buf;
                    return WithDecimals(d, c.Decimals, _neg);
                }
                if (!TryReadValue(c, out var v)) { err = true; return "E-Ad"; }
                var s = WithDecimals(Math.Abs(v).ToString(), c.Decimals, v < 0);
                return SevenSeg.GlyphCount(s) > digits ? "----------".Substring(0, digits) : s;
            default: return "";
        }
    }

    static string WithDecimals(string digits, int dec, bool neg)
    {
        if (dec > 0)
        {
            digits = digits.PadLeft(dec + 1, '0');
            digits = digits.Insert(digits.Length - dec, ".");
        }
        return (neg ? "-" : "") + digits;
    }

    public void Refresh()
    {
        Array.Clear(Segments);
        var pg = CurrentPage;
        string status = $"Trang {PageIndex + 1}" + (Level > 0 ? $" · cấp {Level}" : "");
        if (_msgMs > 0 && _msg.Length > 0)
        {
            var seg = SevenSeg.Layout(_msg, 4); seg.CopyTo(Segments, 0);
        }
        else if (_promptLevel > 0)
        {
            SevenSeg.Layout("PASS", 4).CopyTo(Segments, 0);
            SevenSeg.Layout(new string('-', _buf.Length), 4).CopyTo(Segments, 4);
        }
        else if (pg != null)
        {
            if (_p.Layout.HasUpper)
            {
                int n = pg.Upper.Span8 ? 8 : 4;
                var t = FormatCell(pg.Upper, n, out _);
                var seg = SevenSeg.Layout(t, n);
                seg.CopyTo(Segments, 0);
            }
            if (_p.Layout.HasLower && !pg.Upper.Span8)
            {
                var t = FormatCell(pg.Lower, 4, out _);
                SevenSeg.Layout(t, 4).CopyTo(Segments, 4);
            }
        }
        Status = status;

        for (int i = 0; i < 8; i++)
        {
            var l = _p.Leds.FirstOrDefault(x => x.Index == i + 1);
            bool lit = false;
            if (l != null && l.Present)
            {
                switch (l.Mode)
                {
                    case LedMode.Steady: lit = ReadBit(l.Address) ^ l.Invert; break;
                    case LedMode.Blink: lit = (ReadBit(l.Address) ^ l.Invert) && _blink; break;
                    case LedMode.Comm: lit = true; break;
                }
            }
            LedLit[i] = lit;
        }
    }
}
