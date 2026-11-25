param([string]$Dir = "", [string]$Script = "s1.txt", [int]$Timeout = 300, [string]$Affinity = "")
# Starts the GUI minimized without taking the focus (cmd start /min), with the startup script set in its config.ini,
# and waits for it to end (the script ends with quit). -Affinity <hex mask>: the GUI and the engines it starts run on
# those logical CPUs only (cmd start /affinity). -Dir: the folder of the GUI (default: _test\run-b1 of this repository).
# The run has passed (exit code 0) only when the GUI ended by itself before the time limit with exit code 0 and every
# file named by "screenshot <file>" and "savelog <file>" in the script was written during the run; else the reason is
# written and the exit code is 1. The time and the peak memory are written in both cases.
if (-not $Dir) { $Dir = Join-Path (Split-Path -Parent $PSScriptRoot) "_test\run-b1" }
$ini = Join-Path $Dir "config.ini"
$text = Get-Content -Raw -Encoding UTF8 $ini
$text = [regex]::Replace($text, "(?m)^startup=.*$", "startup=$Script")
[System.IO.File]::WriteAllText($ini, $text, (New-Object System.Text.UTF8Encoding($false)))
# the files the script writes (relative names: in the folder of the script)
$scriptPath = if ([System.IO.Path]::IsPathRooted($Script)) { $Script } else { Join-Path $Dir $Script }
$expected = @()
foreach ($l in (Get-Content -Encoding UTF8 $scriptPath)) {
	if ($l.Trim() -match '^(screenshot|savelog)\s+(.+)$') {
		$f = $Matches[2].Trim()
		$expected += if ([System.IO.Path]::IsPathRooted($f)) { $f } else { Join-Path (Split-Path -Parent $scriptPath) $f }
	}
}
# the peaks the system keeps and the exit code, read through the handle after the end (a sample taken while it runs
# misses the last one)
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public static class NgPeak {
	[StructLayout(LayoutKind.Sequential)] public struct Counters { public uint cb, PageFaultCount; public UIntPtr PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage; }
	[DllImport("psapi.dll")] static extern bool GetProcessMemoryInfo(IntPtr process, out Counters c, uint cb);
	[DllImport("kernel32.dll")] static extern bool GetExitCodeProcess(IntPtr process, out uint code);
	public static double[] Read(IntPtr process) {
		Counters c;
		if (!GetProcessMemoryInfo(process, out c, (uint)Marshal.SizeOf(typeof(Counters)))) return new double[] { 0, 0 };
		return new double[] { c.PeakWorkingSetSize.ToUInt64(), c.PeakPagefileUsage.ToUInt64() };
	}
	public static long ExitCode(IntPtr process) {
		uint code;
		return GetExitCodeProcess(process, out code) ? (long)code : -1;
	}
}
'@
$exe = Join-Path $Dir "nikque-gui.exe"
$t0 = Get-Date
# started from a hidden cmd of its own: a "cmd /c start" here would hand the GUI the pipe of this output when the
# output is taken by the caller, and the caller would wait for the GUI to end
$aff = if ($Affinity) { "/affinity $Affinity " } else { "" }
Start-Process -FilePath cmd.exe -ArgumentList "/c start `"`" /min $aff`"$exe`"" -WindowStyle Hidden
# a file written a moment ago may start late (it is scanned first): up to 30 seconds
$p = $null; $handle = $null
for ($i = 0; $i -lt 300 -and -not $handle; ++$i) {
	Start-Sleep -Milliseconds 100
	$p = Get-Process nikque-gui -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe } | Select-Object -First 1
	if ($p) { try { $handle = $p.Handle } catch { $p = $null } }  # (kept open: the counters stay readable after the end)
}
if (-not $handle) { "FAIL not started"; exit 1 }
$timedOut = $false
while (-not $p.HasExited) {
	Start-Sleep -Milliseconds 500
	if (((Get-Date) - $t0).TotalSeconds -gt $Timeout) { $timedOut = $true; $p.Kill(); break }
}
$p.WaitForExit()
$peak, $priv = [NgPeak]::Read($handle)
$code = [NgPeak]::ExitCode($handle)
$why = @()
if ($timedOut) { $why += "timeout: killed" }
elseif ($code -ne 0) { $why += "exit code $code" }
foreach ($f in $expected) {
	$item = Get-Item -LiteralPath $f -ErrorAction SilentlyContinue
	if (-not $item -or $item.Length -eq 0 -or $item.LastWriteTime -lt $t0) { $why += "missing: $f" }
}
$sum = "elapsed {0:N1} s, peak working set {1:N1} MB, peak private {2:N1} MB" -f ((Get-Date) - $t0).TotalSeconds, ($peak / 1MB), ($priv / 1MB)
if ($why.Count) { "FAIL " + ($why -join "; ") + " ($sum)"; exit 1 }
$sum
exit 0
