param([string]$Dir = "D:\nikque-gui\_test\run-b1", [string]$Script = "s1.txt", [int]$Timeout = 300, [string]$Affinity = "")
# Starts the GUI minimized without taking the focus (cmd start /min), with the startup script set in its config.ini,
# and waits for it to end (the script ends with quit). -Affinity <hex mask>: the GUI and the engines it starts run on
# those logical CPUs only (cmd start /affinity).
$ini = Join-Path $Dir "config.ini"
$text = Get-Content -Raw -Encoding UTF8 $ini
$text = [regex]::Replace($text, "(?m)^startup=.*$", "startup=$Script")
[System.IO.File]::WriteAllText($ini, $text, (New-Object System.Text.UTF8Encoding($false)))
$exe = Join-Path $Dir "nikque-gui.exe"
$t0 = Get-Date
# started from a hidden cmd of its own: a "cmd /c start" here would hand the GUI the pipe of this output when the
# output is taken by the caller, and the caller would wait for the GUI to end
$aff = if ($Affinity) { "/affinity $Affinity " } else { "" }
Start-Process -FilePath cmd.exe -ArgumentList "/c start `"`" /min $aff`"$exe`"" -WindowStyle Hidden
# a file written a moment ago may start late (it is scanned first): up to 30 seconds
$p = $null
for ($i = 0; $i -lt 60 -and -not $p; ++$i) {
	Start-Sleep -Milliseconds 500
	$p = Get-Process nikque-gui -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }
}
if (-not $p) { "not started"; exit 1 }
# the peaks the system keeps, read through the handle after the end (a sample taken while it runs misses the last one)
Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public static class NgPeak {
	[StructLayout(LayoutKind.Sequential)] public struct Counters { public uint cb, PageFaultCount; public UIntPtr PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage; }
	[DllImport("psapi.dll")] static extern bool GetProcessMemoryInfo(IntPtr process, out Counters c, uint cb);
	public static double[] Read(IntPtr process) {
		Counters c;
		if (!GetProcessMemoryInfo(process, out c, (uint)Marshal.SizeOf(typeof(Counters)))) return new double[] { 0, 0 };
		return new double[] { c.PeakWorkingSetSize.ToUInt64(), c.PeakPagefileUsage.ToUInt64() };
	}
}
'@
$handle = $p.Handle  # (kept open: the counters stay readable after the end)
while (-not $p.HasExited) {
	Start-Sleep -Milliseconds 500
	if (((Get-Date) - $t0).TotalSeconds -gt $Timeout) { "timeout: killed"; $p.Kill(); break }
}
$p.WaitForExit()
$peak, $priv = [NgPeak]::Read($handle)
"elapsed {0:N1} s, peak working set {1:N1} MB, peak private {2:N1} MB" -f ((Get-Date) - $t0).TotalSeconds, ($peak / 1MB), ($priv / 1MB)
