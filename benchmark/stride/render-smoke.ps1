# Render sanity smoke for the Stride benchmark: starts the exe in render mode for each scenario, takes a screenshot of the window
# (screen copy of the client area), and fails when the picture is empty (non-background pixel fraction below a threshold) or, for
# --camera=sky, when it is not empty. Catches "empty scene" regressions (camera, light, material, culling).
# Usage: powershell -File render-smoke.ps1 -Exe <StrideBench.exe> [-OutDir <dir under .tmp>] [-Quick] [-Keep]
#   -Quick: pit-2000, chains, mixed, sky only. -Keep: keep the PNGs (default: deleted after the check).
# The window must be visible and not covered. Exit code 0 = all passed.
param([Parameter(Mandatory=$true)][string]$Exe, [string]$OutDir = ".tmp/render-smoke", [switch]$Quick, [switch]$Keep)
$MinContent = 0.02   # lowest fraction seen on a good frame: raycast-1000 0.047, scale-64000 0.062
$MaxSky = 0.01       # sky frame: good frames showed 0.000 to 0.002
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { 
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X,Y; }
}
"@
Add-Type -AssemblyName System.Drawing
[W]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$cases = @(
  @{n='pit-2000'; a='--scenario=pit-2000'; d=8},
  @{n='chains'; a='--scenario=chains'; d=8},
  @{n='mixed'; a='--scenario=mixed'; d=10},
  @{n='sky-pit-2000'; a='--scenario=pit-2000 --camera=sky'; d=8; sky=$true})
if (-not $Quick) { $cases += @(
  @{n='raycast-1000'; a='--scenario=raycast-1000'; d=8},
  @{n='raycast-10000'; a='--scenario=raycast-10000'; d=10},
  @{n='unlock-render-mixed'; a='--scenario=unlock-render-mixed'; d=10},
  @{n='pit-2000-stress'; a='--scenario=pit-2000 --profile=stress'; d=8},
  @{n='scale-1000'; a='--scenario=scale --bodies=1000'; d=8},
  @{n='scale-4000'; a='--scenario=scale --bodies=4000'; d=10},
  @{n='scale-16000'; a='--scenario=scale --bodies=16000'; d=20},
  @{n='scale-64000'; a='--scenario=scale --bodies=64000'; d=40},
  @{n='debris-8'; a='--scenario=debris --rate=8'; d=10},
  @{n='debris-pooled-8'; a='--scenario=debris-pooled --rate=8'; d=10}) }
function Frac($png) {
  $b = [System.Drawing.Bitmap]::FromFile($png); $bg = $b.GetPixel(2,2); $n=0; $c=0
  for ($y=0;$y -lt $b.Height;$y+=2){ for($x=0;$x -lt $b.Width;$x+=2){ $p=$b.GetPixel($x,$y); $n++
    if ([math]::Abs($p.R-$bg.R)+[math]::Abs($p.G-$bg.G)+[math]::Abs($p.B-$bg.B) -gt 24){$c++} } }
  $b.Dispose(); return @($bg, ($c/$n)) }
$fail = 0
foreach ($c in $cases) {
  $proc = Start-Process -FilePath $Exe -ArgumentList ("--mode=render --warmup=120 --measure=1 --out=" + $OutDir + " " + $c.a) -PassThru
  $ok = $false
  for ($i=0; $i -lt 120 -and -not $ok; $i++) { $proc.Refresh(); if ($proc.MainWindowHandle -ne 0) { $ok = $true } else { Start-Sleep -Milliseconds 250 } }
  if (-not $ok) { "FAIL $($c.n): no window"; $fail++; Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue; continue }
  Start-Sleep -Seconds $c.d
  $h = $proc.MainWindowHandle; [W]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 500
  $r = New-Object W+RECT; [W]::GetClientRect($h, [ref]$r) | Out-Null
  $pt = New-Object W+POINT; [W]::ClientToScreen($h, [ref]$pt) | Out-Null
  $w = $r.R - $r.L; $hh = $r.B - $r.T
  $bmp = New-Object System.Drawing.Bitmap $w, $hh; $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($pt.X, $pt.Y, 0, 0, $bmp.Size)
  $png = Join-Path $OutDir ("smoke-" + $c.n + ".png"); $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
  Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
  $res = Frac $png; $bg = $res[0]; $f = $res[1]
  $black = ($bg.R + $bg.G + $bg.B) -lt 10
  if ($c.sky) { $pass = (-not $black) -and ($f -le $MaxSky) } else { $pass = (-not $black) -and ($f -ge $MinContent) }
  $tag = if ($pass) { "PASS" } else { $fail++; "FAIL" }
  "{0} {1}: content_fraction={2:F4} bg=({3},{4},{5}) {6}x{7}" -f $tag, $c.n, $f, $bg.R, $bg.G, $bg.B, $w, $hh
  if (-not $Keep) { Remove-Item $png -ErrorAction SilentlyContinue }
}
if ($fail -gt 0) { "render-smoke: $fail FAILED"; exit 1 } else { "render-smoke: all passed"; exit 0 }
