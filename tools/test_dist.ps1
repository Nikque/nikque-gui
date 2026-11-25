param(
	[string]$Dist = "",                                   # the distribution folder (default: the newest dist\nikque-gui-*)
	[string]$Run = "",                                    # the folder of the run (default: _test\run-dist of this repository)
	[string[]]$Scripts = @("s1", "s2", "s3", "s4", "s7", "s8", "s9", "s10", "s11", "s14", "s18", "s19", "s20", "s21", "s22", "s23", "s24", "s25", "s28", "s5", "s29", "s30"),
	[int]$Timeout = 600,
	[string]$Affinity = "F00000"                          # the logical CPUs of the GUI and its engines (20-23)
)
# Runs the command scripts of tests\gui-scripts on a copy of the distribution (its exe, its engines with exe=auto),
# with the small test books: the nikque engine on a fresh copy of _test\books\book2.dat before each script, newedax
# on _test\books\book2.dat. The scripts name the files of the folder they were written in (D:\nikque-gui): the paths
# of its _test\run-b1 are turned into $Run (the inputs are copied), the other paths of its _test into _test of this
# repository. Each script ends with savelog and quit; the time and the peak memory of the GUI, or why the script
# failed (rungui.ps1), are written to $Run\summary.txt. The exit code is 1 when a script failed.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $Run) { $Run = Join-Path $root "_test\run-dist" }
if (-not $Dist) { $Dist = (Get-ChildItem (Join-Path $root "dist") -Directory | Sort-Object LastWriteTime | Select-Object -Last 1).FullName }
if (Test-Path $Run) { Remove-Item -Recurse -Force $Run }
Copy-Item -Recurse $Dist $Run
$b1 = Join-Path $root "_test\run-b1"
foreach ($f in "games.txt", "games-c.txt", "games-short.txt", "p.obf", "big1m.txt") { Copy-Item (Join-Path $b1 $f) $Run }
$sp = Join-Path $Run "空白 の dir"
New-Item -ItemType Directory -Force $sp | Out-Null
foreach ($f in "g a.txt", "g c.txt", "g short.txt", "p q.obf") { Copy-Item -LiteralPath (Join-Path $b1 "空白 の dir\$f") $sp }
$jp = Join-Path $Run "日本語"
New-Item -ItemType Directory -Force $jp | Out-Null
Copy-Item (Join-Path $b1 "games.txt") $jp

# the test settings: those of tests\gui-scripts\config.ini, the engines of the distribution (dir empty, exe=auto;
# the section of the C version is still [nikque18] there, as in a config.ini of 0.2.0, which the GUI reads as [nikque])
$books = Join-Path $root "_test\books"
$copy = Join-Path $Run "book-copy.dat"
$cfg = (Get-Content -Raw -Encoding UTF8 (Join-Path $root "tests\gui-scripts\config.ini")) -split "`r?`n"
$sec = ""
$cfg = foreach ($l in $cfg) {
	if ($l -match '^\[(.+)\]$') { $sec = $Matches[1] }
	if ($sec -in "nikque", "nikque18", "newedax") {
		if ($l -match '^dir=') { "dir="; continue }
		if ($l -match '^exe=') { "exe=auto"; continue }
		if ($l -match '^book=') { if ($sec -ne "newedax") { "book=$copy" } else { "book=$books\book2.dat" }; continue }
	}
	if ($l -eq "[list]") { "[play]"; "end_list=1"; "" }
	$l
}
[System.IO.File]::WriteAllText((Join-Path $Run "config.ini"), ($cfg -join "`r`n"), (New-Object System.Text.UTF8Encoding($false)))

$written = "D:\nikque-gui\_test"
foreach ($f in Get-ChildItem (Join-Path $root "tests\gui-scripts\s*.txt")) {
	$t = Get-Content -Raw -Encoding UTF8 $f.FullName
	$t = $t.Replace("$written\run-b1", $Run).Replace($written, (Join-Path $root "_test"))
	[System.IO.File]::WriteAllText((Join-Path $Run $f.Name), $t, (New-Object System.Text.UTF8Encoding($false)))
}

$summary = Join-Path $Run "summary.txt"
$out = @()
$failed = 0
foreach ($s in $Scripts) {
	Copy-Item -Force (Join-Path $books "book2.dat") $copy
	$t0 = Get-Date
	$res = & (Join-Path $PSScriptRoot "rungui.ps1") -Dir $Run -Script "$s.txt" -Timeout $Timeout -Affinity $Affinity
	if ($LASTEXITCODE -ne 0) { ++$failed }
	$line = "{0,-4} {1}  ({2:HH:mm:ss})" -f $s, ($res -join " "), $t0
	$out += $line
	$line
	[System.IO.File]::WriteAllText($summary, ($out -join "`r`n") + "`r`n", (New-Object System.Text.UTF8Encoding($false)))
}
if ($failed) { "$failed of $($Scripts.Count) scripts failed"; exit 1 }
"all $($Scripts.Count) scripts passed"
