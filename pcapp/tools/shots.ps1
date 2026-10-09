# Chụp ảnh từng tab của PC app để kiểm tra giao diện bằng mắt. Dùng: powershell -File tools\shots.ps1 [số tab]
param([int]$Tabs = 7)
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  public struct RECT { public int L,T,R,B; } }
"@
[W]::SetProcessDPIAware() | Out-Null
$root = Split-Path $PSScriptRoot
$exe = Join-Path $root "src\PanelKbd.App\bin\Debug\net8.0-windows\PanelKbdConfig.exe"
for ($i = 0; $i -lt $Tabs; $i++) {
  $p = Start-Process $exe -ArgumentList "--tab $i" -PassThru
  Start-Sleep 4
  if ($p.HasExited) { "EXITED code $($p.ExitCode)"; exit 1 }
  $p.Refresh()
  [W]::ShowWindow($p.MainWindowHandle, 3) | Out-Null
  Start-Sleep 1
  [W]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
  Start-Sleep -Milliseconds 800
  $r = New-Object W+RECT
  [W]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
  $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
  $bmp.Save((Join-Path $env:TEMP "pk_tab$i.png"))
  Stop-Process $p -Force
}
