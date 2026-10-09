namespace PanelKbd.Core;

public enum Severity { Info, Warning, Error }

public sealed class Issue
{
    public Severity Severity { get; init; }
    public string Where { get; init; } = "";
    public string Message { get; init; } = "";
    /// <summary>Đối tượng (Cell / KeyConfig / LedConfig / Page / null) để nhảy tới khi bấm.</summary>
    public object? Target { get; init; }
    public int PageIndex { get; init; } = -1;
    public string Icon => Severity switch { Severity.Error => "✖", Severity.Warning => "⚠", _ => "ℹ" };
}

/// <summary>Một dòng bảng đối chiếu địa chỉ PLC ↔ Modbus (PC-22).</summary>
public sealed class AddressRow
{
    public string Where { get; init; } = "";
    public string Role { get; init; } = "";
    public string Plc { get; init; } = "";
    public string Table { get; init; } = "";
    public int Modbus { get; init; }
    public string Hex => $"0x{Modbus:X4}";
    public string Classic { get; init; } = "";
    public string Type { get; init; } = "";
}

public sealed class CheckResult
{
    public List<Issue> Issues { get; } = new();
    public List<AddressRow> Map { get; } = new();
    public bool HasErrors => Issues.Any(i => i.Severity == Severity.Error);
}

public static class Validator
{
    public const int MaxImageBytes = RuntimeCompiler.ImageSize;

    public static CheckResult Check(ProjectFile p, PlcProfile profile)
    {
        var r = new CheckResult();
        var c = p.Comm;

        // Cập nhật lại chuỗi giải mã ở các đối tượng, kể cả khi không kiểm tra sâu
        void Err(string where, string msg, object? t = null, int page = -1) =>
            r.Issues.Add(new Issue { Severity = Severity.Error, Where = where, Message = msg, Target = t, PageIndex = page });
        void Warn(string where, string msg, object? t = null, int page = -1) =>
            r.Issues.Add(new Issue { Severity = Severity.Warning, Where = where, Message = msg, Target = t, PageIndex = page });
        void Info(string where, string msg) =>
            r.Issues.Add(new Issue { Severity = Severity.Info, Where = where, Message = msg });

        // --- Kết nối ---
        if (c.PlcStation is < 1 or > 247) Err("Kết nối PLC", "Số trạm PLC phải 1…247");
        if (c.KeyboardStation is < 1 or > 247) Err("Kết nối PLC", "Số trạm bàn phím phải 1…247");
        if (c.Baud is < 2400 or > 115200) Err("Kết nối PLC", "Tốc độ phải 2400…115200");
        if (c.StopBits is not (1 or 2)) Err("Kết nối PLC", "Stop bit phải 1 hoặc 2");
        if (c.TimeoutMs is < 20 or > 5000) Err("Kết nối PLC", "Timeout phải 20…5000 ms");
        if (c.Retries is < 0 or > 5) Err("Kết nối PLC", "Số lần thử lại phải 0…5");
        if (profile.MinTimeoutMs > 0 && c.TimeoutMs < profile.MinTimeoutMs)
            Warn("Kết nối PLC", $"Hồ sơ '{profile.Name}' nên dùng timeout ≥ {profile.MinTimeoutMs} ms (PLC trả lời theo vòng quét), đang là {c.TimeoutMs} ms → dễ báo E-CO.");
        if (c.PollMs is < 10 or > 5000) Err("Kết nối PLC", "Chu kỳ hỏi phải 10…5000 ms");
        if (c.Brightness is < 0 or > 7) Err("Kết nối PLC", "Độ sáng phải 0…7");
        if (c.Baud >= 57600 && c.Parity != ParityMode.None)
            Info("Kết nối PLC", "Tốc độ cao kèm parity: kiểm tra PLC hỗ trợ cùng khung.");
        if (!profile.Verified && c.Mode == OperationMode.Master)
            Warn("Hồ sơ PLC", $"Hồ sơ '{profile.Name}' CHƯA kiểm tra bằng PLC thật — thử không tải trước khi đưa lên máy.");
        if (profile.NeedsUserOffset && c.UserOffset < 0)
            Err("Kết nối PLC", "Địa chỉ bắt đầu phải ≥ 0");

        // --- Bảo mật ---
        foreach (var (name, pw) in new[] { ("vận hành", p.Security.OperatorPassword), ("kỹ thuật", p.Security.TechPassword) })
            if (pw.Length > 0 && (pw.Length > 4 || !pw.All(char.IsDigit)))
                Err("Bảo mật", $"Mật khẩu {name} phải gồm 1–4 chữ số");
        if (p.Security.AutoLogoutSec is < 0 or > 255) Err("Bảo mật", "Tự đăng xuất phải 0…255 s");
        if (p.Security.RunTimeoutSec is < 0 or > 255) Err("Bảo mật", "Thời gian về trang RUN phải 0…255 s");

        // --- Trang ---
        if (p.Pages.Count == 0) Err("Trang", "Cần ít nhất 1 trang");
        if (!p.Layout.HasUpper && !p.Layout.HasLower) Warn("Bố cục", "Board không có hàng 7 đoạn nào — không có gì để hiện.");
        if (p.Pages.Count > ProjectFile.MaxPages) Err("Trang", $"Tối đa {ProjectFile.MaxPages} trang (đang có {p.Pages.Count})");
        if (p.Security.RunPage < 1 || p.Security.RunPage > Math.Max(1, p.Pages.Count))
            Err("Bảo mật", $"Trang RUN {p.Security.RunPage} không tồn tại");

        if (c.Mode == OperationMode.Slave)
        {
            Info("Chế độ", "Chế độ SLAVE: PLC làm master, bàn phím dùng bảng thanh ghi cố định — trang/ô/phím/LED không tải xuống.");
            return r;
        }

        var occupied = new List<(ModbusTable T, int Start, int Count, Cell Cell, string Where, bool Writes)>();

        for (int i = 0; i < p.Pages.Count; i++)
        {
            var pg = p.Pages[i];
            if (p.Layout.HasUpper) CheckCell(pg.Upper, $"Trang {i + 1} · hàng trên", i); else Skipped(pg.Upper);
            if (p.Layout.HasLower) CheckCell(pg.Lower, $"Trang {i + 1} · hàng dưới", i); else Skipped(pg.Lower);
            if (pg.Upper.Span8 && p.Layout.HasUpper && !p.Layout.HasLower)
                Err($"Trang {i + 1}", "Ghép 8 số cần board có cả hai hàng 7 đoạn.", pg.Upper, i);
            if (pg.Upper.Span8 && pg.Lower.Type != CellType.None && p.Layout.HasLower)
                Warn($"Trang {i + 1}", "Hàng trên ghép 8 số nên hàng dưới bị che — đối tượng hàng dưới không hiện.", pg.Lower, i);
            if (!string.IsNullOrEmpty(pg.Name) && SevenSeg.GlyphCount(pg.Name) > 20)
                Warn($"Trang {i + 1}", "Tên trang dài (chỉ để ghi chú trên PC)", pg, i);
        }

        void Skipped(Cell c) { c.ResolvedText = "Board không có hàng này — bỏ qua"; c.ResolvedError = false; }


        void CheckCell(Cell cell, string where, int page)
        {
            cell.ResolvedText = ""; cell.ResolvedError = false;
            if (cell.Type == CellType.None) return;
            if (cell.Type == CellType.Label)
            {
                CheckLabel(cell.Label, "Nhãn", where, cell, page, allowEmpty: false);
                return;
            }

            var res = AddressResolver.Resolve(cell.Address, profile, c);
            if (!res.Ok)
            {
                cell.ResolvedText = res.Error; cell.ResolvedError = true;
                Err(where, $"Địa chỉ '{cell.Address}': {res.Error}", cell, page);
                return;
            }
            var mr = res.Ref;
            cell.ResolvedText = Describe(mr);

            string role = cell.Type switch { CellType.Display => "Hiện số", CellType.Input => "Nhập số", _ => "Trạng thái bit" };

            if (cell.Type == CellType.BitState)
            {
                if (!mr.Table.IsBitTable() && !mr.HasBit)
                    Err(where, $"Trạng thái bit cần coil/đầu vào bit hoặc 'Dxxx.bit' (đang là thanh ghi {mr.Address})", cell, page);
                CheckLabel(cell.OnLabel, "Nhãn ON", where, cell, page, false);
                CheckLabel(cell.OffLabel, "Nhãn OFF", where, cell, page, false);
                r.Map.Add(Row(where, role, cell.Address, mr, "bit"));
                return;
            }

            // Hiện số / nhập số
            if (mr.Table.IsBitTable())
            {
                Err(where, $"{role} cần thanh ghi (word), '{cell.Address}' là bit", cell, page);
                return;
            }
            if (mr.HasBit && cell.DataType.Registers() == 2)
                Err(where, "Địa chỉ có '.bit' không đi cùng kiểu 32 bit", cell, page);
            if (cell.Type == CellType.Input && !mr.Table.IsWritable())
                Err(where, $"Không ghi được vào {mr.Table.Display()} — chọn vùng ghi được", cell, page);

            int regs = cell.DataType.Registers();
            if (mr.Address + regs - 1 > 65535) Err(where, "Địa chỉ 32 bit vượt cuối bảng Modbus", cell, page);
            if (mr.Area != null && regs == 2 && mr.Area.Stride == 1 && mr.Area.Max > 0)
            {
                // Số 32 bit chiếm 2 thanh ghi liên tiếp: ô sau phải vẫn trong vùng
                int numIdx = mr.Address - mr.Area.ModbusStart + mr.Area.Min;
                if (!profile.NeedsUserOffset && numIdx + 1 > mr.Area.Max)
                    Err(where, $"Số 32 bit tại {cell.Address} tràn ra khỏi vùng {mr.Area.Name}", cell, page);
            }

            bool signed = cell.DataType is DataType.Int16 or DataType.Int32;
            long lo = cell.DataType switch { DataType.Int16 => short.MinValue, DataType.Int32 => int.MinValue, _ => 0 };
            long hi = cell.DataType switch { DataType.Int16 => short.MaxValue, DataType.UInt16 => ushort.MaxValue, DataType.Int32 => int.MaxValue, _ => int.MaxValue };

            {
                if (cell.Min > cell.Max) Err(where, $"Min ({cell.Min}) lớn hơn Max ({cell.Max})", cell, page);
                if (cell.Min < lo || cell.Max > hi)
                    Err(where, $"Min/Max vượt phạm vi kiểu {cell.DataType} ({lo}…{hi})", cell, page);
                if (!signed && cell.Min < 0) Err(where, "Kiểu không dấu nhưng Min < 0", cell, page);
            }

            int avail = cell.DigitCount;
            long refMax = cell.Max;
            long refMin = cell.Min;
            int needed = Math.Max(Math.Max(Digits(refMax), Digits(refMin)), cell.Decimals + 1);
            if (refMin < 0) needed++;
            if (needed > avail)
            {
                string msg = $"Cần {needed} ô để hiện {refMin}…{refMax}" + (cell.Decimals > 0 ? $" (thập phân {cell.Decimals})" : "") + $", hàng chỉ có {avail} ô.";
                Err(where, msg + " Giảm Max/Min hoặc ghép 8 số.", cell, page);
            }

            occupied.Add((mr.Table, mr.Address, regs, cell, where, cell.Type == CellType.Input));
            r.Map.Add(Row(where, role, cell.Address, mr, cell.DataType.ToString()));
        }

        void CheckLabel(string text, string what, string where, Cell cell, int page, bool allowEmpty)
        {
            if (!allowEmpty && text.Trim().Length == 0) { Warn(where, $"{what} trống", cell, page); return; }
            var g = SevenSeg.Encode(text, out var bad);
            if (bad.Count > 0)
                Err(where, $"{what} '{text}' có ký tự không hiện được trên 7 đoạn: {string.Join(' ', bad.Distinct())}. Dùng: {SevenSeg.Supported}", cell, page);
            if (SevenSeg.GlyphCount(text) > 4)
                Err(where, $"{what} '{text}' dài hơn 4 ký tự", cell, page);
        }

        // Chồng lấp thanh ghi
        for (int a = 0; a < occupied.Count; a++)
            for (int b = a + 1; b < occupied.Count; b++)
            {
                var x = occupied[a]; var y = occupied[b];
                if (x.T != y.T) continue;
                bool overlap = x.Start < y.Start + y.Count && y.Start < x.Start + x.Count;
                if (!overlap) continue;
                bool same = x.Start == y.Start && x.Count == y.Count;
                if (same)
                {
                    if (x.Writes && y.Writes)
                        Warn(y.Where, $"Hai ô nhập cùng ghi vào {y.Cell.Address} (cũng ở {x.Where})", y.Cell);
                    continue;
                }
                Err(y.Where, $"{y.Cell.Address} ({y.Count} thanh ghi) chồng lên {x.Cell.Address} ({x.Count} thanh ghi) ở {x.Where}", y.Cell);
            }

        // --- Phím ---
        bool canNavigate = p.Keys.Any(k => k.Present && (k.Action is KeyAction.PrevPage or KeyAction.NextPage or KeyAction.GotoPage));
        if (p.Pages.Count > 1 && !canNavigate)
            Warn("Bố cục", $"Có {p.Pages.Count} trang nhưng không có phím nào chuyển trang (▲▼/F1/F2/K3 chưa lắp hoặc chưa gán).");
        if (!p.Layout.IsPresent("ENT") && p.Pages.Any(pg => pg.Upper.Type == CellType.Input || pg.Lower.Type == CellType.Input))
            Warn("Bố cục", "Có ô nhập số nhưng board không có phím ENT để ghi.");
        foreach (var k in p.Keys)
        {
            string where = $"Phím {k.Title}";
            k.ResolvedText = ""; k.ResolvedError = false;
            if (!k.Present) { k.ResolvedText = "Board không có phím này — bỏ qua"; continue; }
            if (k.NeedsAddress)
            {
                var res = AddressResolver.Resolve(k.Address, profile, c);
                if (!res.Ok) { k.ResolvedText = res.Error; k.ResolvedError = true; Err(where, $"Địa chỉ '{k.Address}': {res.Error}", k); }
                else
                {
                    var mr = res.Ref; k.ResolvedText = Describe(mr);
                    if (!mr.Table.IsWritable()) Err(where, $"Không ghi được vào {mr.Table.Display()}", k);
                    else if (k.IsBitAction && !mr.Table.IsBitTable() && !mr.HasBit)
                        Err(where, "Thao tác bit cần coil hoặc 'Dxxx.bit'", k);
                    else if (k.IsBitAction && mr.HasBit)
                        Warn(where, "Ghi bit trong thanh ghi dùng đọc–sửa–ghi (2 lệnh); nếu ladder cùng đổi thanh ghi này có thể bị ghi đè. Ưu tiên coil (M…) nếu PLC có.", k);
                    else if (k.Action == KeyAction.WriteConst)
                    {
                        if (mr.Table.IsBitTable()) Err(where, "Ghi hằng số cần thanh ghi, không phải coil", k);
                        else if (k.Const < short.MinValue || k.Const > ushort.MaxValue) Err(where, "Hằng số ghi 16 bit phải −32768…65535", k);
                    }
                    r.Map.Add(Row(where, k.Action.ToString(), k.Address, mr, k.IsBitAction ? "bit" : "word"));
                }
            }
            if (k.NeedsPage && (k.TargetPage < 1 || k.TargetPage > p.Pages.Count))
                Err(where, $"Chuyển tới trang {k.TargetPage} không tồn tại (có {p.Pages.Count} trang)", k);
            if (k.Action is KeyAction.Hold or KeyAction.Toggle or KeyAction.Set or KeyAction.Reset or KeyAction.WriteConst)
                Info(where, "Không dùng phím F làm dừng khẩn cấp — mạch an toàn phải đi dây cứng.");
        }

        // --- LED ---
        foreach (var l in p.Leds)
        {
            string where = l.Title;
            l.ResolvedText = ""; l.ResolvedError = false;
            if (!l.Present) { l.ResolvedText = "Board không có LED này — bỏ qua"; continue; }
            if (!l.NeedsAddress) continue;
            var res = AddressResolver.Resolve(l.Address, profile, c);
            if (!res.Ok) { l.ResolvedText = res.Error; l.ResolvedError = true; Err(where, $"Địa chỉ '{l.Address}': {res.Error}", l); continue; }
            var mr = res.Ref; l.ResolvedText = Describe(mr);
            if (!mr.Table.IsBitTable() && !mr.HasBit) Err(where, "LED cần bit (coil, đầu vào bit hoặc 'Dxxx.bit')", l);
            r.Map.Add(Row(where, l.Mode.ToString(), l.Address, mr, "bit"));
        }

        // --- Dung lượng ---
        var comp = RuntimeCompiler.TryBuild(p, profile, out var image, out _);
        if (comp && image != null)
        {
            if (image.Length > MaxImageBytes) Err("Dung lượng", $"File chạy {image.Length} B vượt {MaxImageBytes} B");
        }
        return r;
    }

    static int Digits(long v) => Math.Abs(v).ToString().Length;

    public static string Describe(ModbusRef m)
        => $"→ {m.Table.Display()} · {m.Address} (0x{m.Address:X4}) · {m.Classic}" + (m.HasBit ? $" · bit {m.Bit}" : "");

    static AddressRow Row(string where, string role, string plc, ModbusRef m, string type) => new()
    {
        Where = where, Role = role, Plc = plc, Table = m.Table.Display(), Modbus = m.Address, Classic = m.Classic, Type = type,
    };
}
