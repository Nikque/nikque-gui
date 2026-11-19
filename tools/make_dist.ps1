param(
	# the ZIPs of the engines as they are attached to their releases (not the bin\ of a worktree: the eval.dat in git is
	# not the one of every release; newedax.13 needs the eval.dat of its own ZIP)
	[Parameter(Mandatory)] [string]$NikqueZip,    # edax-4.5.5-nikque.<n>.zip
	[Parameter(Mandatory)] [string]$NewedaxZip,   # edax-4.5.5-newedax.<n>-64.zip
	# what the releases say (gh release view <tag> --json assets: the digest and the size; git ls-remote: the commit of the
	# tag; the size of bin\data\eval.dat in the ZIP)
	[string]$NikqueName = "v4.5.5-nikque.30",
	[string]$NikqueSha = "dec0b41b191aa4a01e6cbb83df997aabe54b01f1b2f91d5d5f48c3ce6c04317c",
	[long]$NikqueSize = 17924857,
	[string]$NikqueCommit = "de85e0859bdd104de4621182306cd00950587157",
	[long]$NikqueEvalSize = 13952436,
	[string]$NewedaxName = "v4.5.5-heath.17",
	[string]$NewedaxSha = "35d2512a7ff4ab863662e43163926048e2eb7c049b6e03b9324eaeb4f61fcd03",
	[long]$NewedaxSize = 68121950,
	[string]$NewedaxCommit = "a9b10155ce34bffeaf763c36dfe374aef49497eb",
	[long]$NewedaxEvalSize = 107612578,
	[string]$Out = "D:\nikque-gui\dist",
	[string]$Eng = ""   # the new empty folder the ZIPs are unpacked in (default: $Out\engines-<version>)
)
# Makes the distribution of nikque GUI: a folder nikque-gui-<version> and its ZIP in $Out.
#   nikque-gui.exe, config.ini (res\default-config.ini), readme.txt, LICENSE.txt, lang\*.txt, manual\ (ja, en, images)
#   engines\nikque and engines\newedax: wEdax-x86-64-v4.exe, -v3.exe, wEdax-x86-64.exe (exe=auto chooses among them),
#   data\eval.dat, LICENSE.txt (GPL v3 of the engine) and SOURCE.txt (where the source is).
# The engines come from the ZIPs of their releases: the SHA-256 and the size of each ZIP are checked against the values
# of its release, it is unpacked in a new empty folder, and its bin\ exe files, bin\data\eval.dat (whose size is checked)
# and LICENSE are copied (not its book.dat nor config.ini). The copies are checked to be the bytes of the ZIP.
# The GUI is taken from build\Release (build.cmd): the measuring build (NG_MEASURE) is refused.
#   opening.ini (res\opening.ini: the names of the openings, without note lines).
# The texts of the GUI (.txt, .ini, .md outside engines\) are written with CRLF, as in the releases before.
# No book, no test files. A list of the files with their SHA-256 is written next to the ZIP.
# Nothing made before is overwritten: the script stops if the folder, the ZIP or $Eng is there.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cml = Get-Content -Raw (Join-Path $root "CMakeLists.txt")
if ($cml -notmatch "project\(nikque_gui VERSION ([0-9.]+)") { throw "no version in CMakeLists.txt" }
$ver = $Matches[1]
$cache = Get-Content -Raw (Join-Path $root "build\CMakeCache.txt")
if ($cache -notmatch "(?m)^NG_MEASURE:BOOL=OFF") { throw "build\ is not the default build (NG_MEASURE is on)" }
$gui = Join-Path $root "build\Release\nikque-gui.exe"
if (-not (Test-Path $gui)) { throw "build\Release\nikque-gui.exe is not built" }
if ((Get-Item $gui).VersionInfo.ProductVersion -ne $ver) { throw "nikque-gui.exe is not $ver (build it again)" }

$name = "nikque-gui-$ver"
$dst = Join-Path $Out $name
$zip = Join-Path $Out "$name.zip"
if (-not $Eng) { $Eng = Join-Path $Out "engines-$ver" }
foreach ($p in $dst, $zip, $Eng) { if (Test-Path $p) { throw "$p is there (made before: not overwritten)" } }
$Engines = [ordered]@{
	nikque  = @{ Zip = $NikqueZip; Sha = $NikqueSha; Size = $NikqueSize; EvalSize = $NikqueEvalSize
		Title = "Edax (C version, nikque)"; Repo = "https://github.com/Nikque/edax-reversi-AVX"; Tag = $NikqueName; Commit = $NikqueCommit }
	newedax = @{ Zip = $NewedaxZip; Sha = $NewedaxSha; Size = $NewedaxSize; EvalSize = $NewedaxEvalSize
		Title = "Heath (formerly newedax, C++ version of Edax)"; Repo = "https://github.com/Nikque/Heath"; Tag = $NewedaxName; Commit = $NewedaxCommit }
}

# the ZIPs of the engines: SHA-256 and size, then unpacked
New-Item -ItemType Directory -Force $Out | Out-Null
New-Item -ItemType Directory $Eng | Out-Null
foreach ($k in $Engines.Keys) {
	$en = $Engines[$k]
	$z = Get-Item $en.Zip
	# the ZIP is of the version named (edax-4.5.5-heath.17-64.zip for v4.5.5-heath.17): the values below are of that release
	$ver = $en.Tag -replace '^v', ''
	if ($z.Name -notmatch ('^edax-' + [regex]::Escape($ver) + '(-\d+)?\.zip$')) { throw "$($z.Name) is not the ZIP of $($en.Tag) (give -${k}Name and the values of its release)" }
	$h = (Get-FileHash -Algorithm SHA256 $z.FullName).Hash.ToLower()
	if ($h -ne $en.Sha.ToLower() -or $z.Length -ne $en.Size) { throw "$($z.Name): SHA-256 $h, $($z.Length) bytes (not the values of the release)" }
	"zip ok  $h  $($z.Length)  $($z.Name)"
	$dir = Join-Path $Eng $k
	New-Item -ItemType Directory $dir | Out-Null
	Expand-Archive -Path $z.FullName -DestinationPath $dir
	$ev = Get-Item (Join-Path $dir "bin\data\eval.dat")
	if ($ev.Length -ne $en.EvalSize) { throw "$k\bin\data\eval.dat: $($ev.Length) bytes, not $($en.EvalSize) (not the eval file of this release)" }
	$en.Tree = $dir
}

New-Item -ItemType Directory -Force (Join-Path $dst "lang") | Out-Null
Copy-Item $gui $dst
Copy-Item (Join-Path $root "readme.txt") $dst
Copy-Item (Join-Path $root "LICENSE.txt") $dst
Copy-Item (Join-Path $root "res\default-config.ini") (Join-Path $dst "config.ini")
Copy-Item (Join-Path $root "res\opening.ini") (Join-Path $dst "opening.ini")
Copy-Item (Join-Path $root "lang\*.txt") (Join-Path $dst "lang")
Copy-Item -Recurse (Join-Path $root "manual") $dst
# the texts of the GUI with CRLF, whatever the line ends of the worktree are (as the releases before)
$texts = @(Get-ChildItem -Recurse -File $dst | Where-Object { $_.Extension -in ".txt", ".ini", ".md" })
foreach ($t in $texts) {
	$b = [System.IO.File]::ReadAllBytes($t.FullName)
	$s = [System.Text.Encoding]::Latin1.GetString($b).Replace("`r`n", "`n").Replace("`n", "`r`n")   # (bytes kept: UTF-8 as it is)
	[System.IO.File]::WriteAllBytes($t.FullName, [System.Text.Encoding]::Latin1.GetBytes($s))
}

function Copy-Engine([string]$folder) {
	$x = $Engines[$folder]
	$tree = $x.Tree
	$edir = Join-Path $dst "engines\$folder"
	New-Item -ItemType Directory -Force (Join-Path $edir "data") | Out-Null
	$pairs = @(@("bin\wEdax-x86-64-v4.exe", "wEdax-x86-64-v4.exe"), @("bin\wEdax-x86-64-v3.exe", "wEdax-x86-64-v3.exe"),
		@("bin\wEdax-x86-64.exe", "wEdax-x86-64.exe"), @("bin\data\eval.dat", "data\eval.dat"), @("LICENSE", "LICENSE.txt"))
	foreach ($p in $pairs) { Copy-Item (Join-Path $tree $p[0]) (Join-Path $edir $p[1]) }
	$text = @(
		"$($x.Title) $($x.Tag)"
		""
		"This engine is free software under the GNU General Public License version 3 (LICENSE.txt)."
		"It runs as a separate program; nikque GUI (MIT License) starts it and talks to it with text commands."
		"このエンジンは GNU GPL v3（LICENSE.txt）の自由なソフトウェアです。別のプログラムとして動き、"
		"nikque GUI（MIT）が起動して文字の命令でやりとりします。"
		""
		"Source / ソース: $($x.Repo)"
		"Tag / タグ: $($x.Tag)"
		"Commit: $($x.Commit)"
		"Files / ファイル: wEdax-x86-64-v4.exe (AVX-512), wEdax-x86-64-v3.exe (AVX2), wEdax-x86-64.exe (other x64 CPUs), data\eval.dat"
	)
	[System.IO.File]::WriteAllText((Join-Path $edir "SOURCE.txt"), ($text -join "`r`n") + "`r`n", (New-Object System.Text.UTF8Encoding($false)))
	# the copies are the bytes unpacked from the ZIP
	foreach ($p in $pairs) {
		$a = (Get-FileHash -Algorithm SHA256 (Join-Path $tree $p[0])).Hash
		$b = (Get-FileHash -Algorithm SHA256 (Join-Path $edir $p[1])).Hash
		if ($a -ne $b) { throw "$folder\$($p[1]) differs from the zip" }
	}
}
foreach ($k in $Engines.Keys) { Copy-Engine $k }

# checks: nothing for tests or measuring, no folder of the build machine in the GUI
$bad = Get-ChildItem -Recurse $dst | Where-Object { $_.Name -match '^(book\.dat|kept\.txt|config-test.*|.*\.pdb|.*\.log|measure.*)$' -or $_.FullName.Substring($dst.Length) -match '\\_test\\' -or ($_.Name -eq "opening.ini" -and $_.DirectoryName -ne $dst) }
if ($bad) { throw "files that must not be in the distribution: $($bad.FullName -join ', ')" }
# the only .dat files: the eval files of the two engines
$dat = @(Get-ChildItem -Recurse -File $dst -Filter *.dat | ForEach-Object { $_.FullName.Substring($dst.Length + 1) })
if ($dat.Count -ne 2 -or ($dat | Where-Object { $_ -notmatch '^engines\\(nikque|newedax)\\data\\eval\.dat$' })) { throw ".dat files: $($dat -join ', ')" }
# the texts of the GUI: CRLF only (no line ending with LF alone)
foreach ($t in $texts) {
	$s = [System.Text.Encoding]::Latin1.GetString([System.IO.File]::ReadAllBytes($t.FullName))
	if ($s -match "(?<!`r)`n") { throw "$($t.FullName) has a line ending with LF alone" }
}
# the opening names: res\opening.ini only, without the note lines ("//")
if (Select-String -Path (Join-Path $dst "opening.ini") -Pattern '^[\s　]*//' -Quiet) { throw "opening.ini holds note lines" }
$bytes = [System.IO.File]::ReadAllBytes($gui)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
$wide = [System.Text.Encoding]::Unicode.GetString($bytes)
foreach ($w in "measure-trace", "NG_MEASURE", "nikque-gui\build", "nikque-gui\src", "_test", "edaxwork") {
	if ($ascii.IndexOf($w, [StringComparison]::OrdinalIgnoreCase) -ge 0 -or $wide.IndexOf($w, [StringComparison]::OrdinalIgnoreCase) -ge 0) { throw "nikque-gui.exe holds '$w'" }
}
"checks ok: .dat = $($dat -join ', ')"

Compress-Archive -Path $dst -DestinationPath $zip -CompressionLevel Optimal
$list = Join-Path $Out "$name-files.txt"
$lines = @("$name  (" + (Get-Date -Format "yyyy-MM-dd HH:mm") + ")", "")
foreach ($f in Get-ChildItem -Recurse -File $dst | Sort-Object FullName) {
	$rel = $f.FullName.Substring($dst.Length + 1)
	$lines += "{0}  {1,12:N0}  {2}" -f (Get-FileHash -Algorithm SHA256 $f.FullName).Hash.ToLower(), $f.Length, $rel
}
$z = Get-Item $zip
$lines += "", ("{0}  {1,12:N0}  {2}" -f (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower(), $z.Length, $z.Name)
[System.IO.File]::WriteAllText($list, ($lines -join "`r`n") + "`r`n", (New-Object System.Text.UTF8Encoding($false)))
$lines
