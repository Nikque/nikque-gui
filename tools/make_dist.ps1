param(
	[Parameter(Mandatory)] [string]$Nikque,    # a worktree of the C version (Edax nikque) whose bin\ holds the release build
	[Parameter(Mandatory)] [string]$Newedax,   # a worktree of newedax whose bin\ holds the release build
	[string]$NikqueName = "v4.5.5-nikque.19",
	[string]$NewedaxName = "v4.5.5-newedax.4",
	[string]$Out = "D:\nikque-gui\dist"
)
# Makes the distribution of nikque GUI: a folder nikque-gui-<version> and its ZIP in $Out.
#   nikque-gui.exe, config.ini (res\default-config.ini), readme.txt, LICENSE.txt, lang\*.txt, manual\ (ja, en, images)
#   engines\nikque18 and engines\newedax: wEdax-x86-64-v4.exe, -v3.exe, wEdax-x86-64.exe (exe=auto chooses among them),
#   data\eval.dat, LICENSE.txt (GPL v3 of the engine) and SOURCE.txt (where the source is).
# The GUI is taken from build\Release (build.cmd): the measuring build (NG_MEASURE) is refused.
# No book, no opening.ini, no test files. A list of the files with their SHA-256 is written next to the ZIP.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$cml = Get-Content -Raw (Join-Path $root "CMakeLists.txt")
if ($cml -notmatch "project\(nikque_gui VERSION ([0-9.]+)") { throw "no version in CMakeLists.txt" }
$ver = $Matches[1]
$cache = Get-Content -Raw (Join-Path $root "build\CMakeCache.txt")
if ($cache -notmatch "(?m)^NG_MEASURE:BOOL=OFF") { throw "build\ is not the default build (NG_MEASURE is on)" }
$gui = Join-Path $root "build\Release\nikque-gui.exe"
if (-not (Test-Path $gui)) { throw "build\Release\nikque-gui.exe is not built" }

$name = "nikque-gui-$ver"
$dst = Join-Path $Out $name
$zip = Join-Path $Out "$name.zip"
New-Item -ItemType Directory -Force $Out | Out-Null
if (Test-Path $dst) { Remove-Item -Recurse -Force $dst }
if (Test-Path $zip) { Remove-Item -Force $zip }
New-Item -ItemType Directory -Force (Join-Path $dst "lang") | Out-Null

Copy-Item $gui $dst
Copy-Item (Join-Path $root "readme.txt") $dst
Copy-Item (Join-Path $root "LICENSE.txt") $dst
Copy-Item (Join-Path $root "res\default-config.ini") (Join-Path $dst "config.ini")
Copy-Item (Join-Path $root "lang\*.txt") (Join-Path $dst "lang")
Copy-Item -Recurse (Join-Path $root "manual") $dst

function Copy-Engine([string]$tree, [string]$folder, [string]$title, [string]$repo, [string]$tag) {
	$e = Join-Path $dst "engines\$folder"
	New-Item -ItemType Directory -Force (Join-Path $e "data") | Out-Null
	foreach ($x in "wEdax-x86-64-v4.exe", "wEdax-x86-64-v3.exe", "wEdax-x86-64.exe") {
		Copy-Item (Join-Path $tree "bin\$x") $e
	}
	Copy-Item (Join-Path $tree "bin\data\eval.dat") (Join-Path $e "data")
	Copy-Item (Join-Path $tree "LICENSE") (Join-Path $e "LICENSE.txt")
	$commit = (git -C $tree rev-parse HEAD).Trim()
	$text = @(
		"$title $tag"
		""
		"This engine is free software under the GNU General Public License version 3 (LICENSE.txt)."
		"It runs as a separate program; nikque GUI (MIT License) starts it and talks to it with text commands."
		"このエンジンは GNU GPL v3（LICENSE.txt）の自由なソフトウェアです。別のプログラムとして動き、"
		"nikque GUI（MIT）が起動して文字の命令でやりとりします。"
		""
		"Source / ソース: $repo"
		"Tag / タグ: $tag"
		"Commit: $commit"
		"Files / ファイル: wEdax-x86-64-v4.exe (AVX-512), wEdax-x86-64-v3.exe (AVX2), wEdax-x86-64.exe (other x64 CPUs), data\eval.dat"
	)
	[System.IO.File]::WriteAllText((Join-Path $e "SOURCE.txt"), ($text -join "`r`n") + "`r`n", (New-Object System.Text.UTF8Encoding($false)))
}
Copy-Engine $Nikque "nikque18" "Edax (C version, nikque)" "https://github.com/Nikque/edax-reversi-AVX" $NikqueName
Copy-Engine $Newedax "newedax" "newedax (C++ version of Edax)" "https://github.com/Nikque/newedax" $NewedaxName

# checks: nothing for tests or measuring, no folder of the build machine in the GUI
$bad = Get-ChildItem -Recurse $dst | Where-Object { $_.Name -match '^(opening\.ini|book\.dat|kept\.txt|config-test.*|.*\.pdb|.*\.log|measure.*)$' -or $_.FullName.Substring($dst.Length) -match '\\_test\\' }
if ($bad) { throw "files that must not be in the distribution: $($bad.FullName -join ', ')" }
$bytes = [System.IO.File]::ReadAllBytes($gui)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
$wide = [System.Text.Encoding]::Unicode.GetString($bytes)
foreach ($w in "measure-trace", "NG_MEASURE", "nikque-gui\build", "nikque-gui\src", "_test") {
	if ($ascii.Contains($w) -or $wide.Contains($w)) { throw "nikque-gui.exe holds '$w'" }
}

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
