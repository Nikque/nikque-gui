param([string]$Dir = "D:\nikque-gui\_test\run-b1", [string]$Script = "s1.txt", [int]$Timeout = 300)
# Starts the GUI minimized without taking the focus (cmd start /min), with the startup script set in its config.ini,
# and waits for it to end (the script ends with quit).
$ini = Join-Path $Dir "config.ini"
$text = Get-Content -Raw -Encoding UTF8 $ini
$text = [regex]::Replace($text, "(?m)^startup=.*$", "startup=$Script")
[System.IO.File]::WriteAllText($ini, $text, (New-Object System.Text.UTF8Encoding($false)))
$exe = Join-Path $Dir "nikque-gui.exe"
$t0 = Get-Date
cmd /c start "" /min "$exe"
Start-Sleep -Milliseconds 500
$p = Get-Process nikque-gui -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }
if (-not $p) { "not started"; exit 1 }
$peak = 0
while (-not $p.HasExited) {
	Start-Sleep -Milliseconds 500
	try { $p.Refresh(); if ($p.WorkingSet64 -gt $peak) { $peak = $p.WorkingSet64 } } catch {}
	if (((Get-Date) - $t0).TotalSeconds -gt $Timeout) { "timeout: killed"; $p.Kill(); break }
}
"elapsed {0:N1} s, peak working set {1:N1} MB" -f ((Get-Date) - $t0).TotalSeconds, ($peak / 1MB)
