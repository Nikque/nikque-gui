#!/bin/bash
# The macOS distribution of nikque GUI (run by .github/workflows/mac.yml after the build):
#   dist/nikque-gui-<version>-mac/  and  dist/nikque-gui-<version>-mac.zip
# It holds "nikque GUI.app" (Apple silicon and Intel), config.ini (res/default-config.ini with the names of macOS in its
# notes), opening.ini, readme.txt, LICENSE.txt, lang/, manual/, and the two engines in engines/nikque and
# engines/newedax: mEdax-arm64 (Apple silicon), mEdax-x64-modern (Intel), data/eval.dat, LICENSE.txt, SOURCE.txt.
#   nikque: the files of its release ZIP (checked by its SHA-256 and size), as they are.
#   newedax: its release ZIP has no engine for macOS: the engines are built here from the source of the release's tag
#            (ne/, checked out by the workflow at that tag; the commit is checked), with data/eval.dat of the release ZIP.
# usage: BOOST_DIR=<Boost 1.92> tools/make_dist_mac.sh <app> <nikque zip> <newedax zip> <newedax source>
set -euo pipefail

APP=$1 NZIP=$2 EZIP=$3 NESRC=$(cd "$4" && pwd)
[ -d "${BOOST_DIR:-}/boost" ] || { echo "BOOST_DIR: the folder of Boost 1.92 (its headers) for newedax"; exit 1; }
NIKQUE_TAG=v4.5.5-nikque.27
NIKQUE_SHA=6b60390cc3143cadffd61b01f6b0c78379bfccdcb2b1a26fbc7906cff454eceb
NIKQUE_SIZE=17880449
NIKQUE_COMMIT=48cf70426ae6aee1641fe418cbcf307272e32d03
NIKQUE_EVAL=13952436
NEWEDAX_TAG=v4.5.5-newedax.14
NEWEDAX_SHA=336068c8173473d974e2fd9b73ac64fd0ee30c66c51c183c84c3f712cfdad203
NEWEDAX_SIZE=68094345
NEWEDAX_COMMIT=87361b8b46c1976f759f645b547c6dac40535dbb
NEWEDAX_EVAL=107612578

VER=$(sed -n 's/^project(nikque_gui VERSION \([0-9.]*\) .*/\1/p' CMakeLists.txt)
[ -n "$VER" ] || { echo "no version in CMakeLists.txt"; exit 1; }
NAME=nikque-gui-$VER-mac
DST=dist/$NAME
[ ! -e "$DST" ] || { echo "$DST is there"; exit 1; }

check_zip() {  # <zip> <sha256> <size>
	local h s
	h=$(shasum -a 256 "$1" | cut -d' ' -f1)
	s=$(stat -f %z "$1")
	[ "$h" = "$2" ] && [ "$s" = "$3" ] || { echo "$1: SHA-256 $h, $s bytes (not the values of the release)"; exit 1; }
	echo "zip ok  $h  $s  $1"
}
check_zip "$NZIP" $NIKQUE_SHA $NIKQUE_SIZE
check_zip "$EZIP" $NEWEDAX_SHA $NEWEDAX_SIZE
[ "$(git -C "$NESRC" rev-parse HEAD)" = $NEWEDAX_COMMIT ] || { echo "$NESRC is not $NEWEDAX_TAG ($NEWEDAX_COMMIT)"; exit 1; }

WORK=$(mktemp -d)
unzip -q "$NZIP" -d "$WORK/n"
unzip -q "$EZIP" -d "$WORK/e"
[ "$(stat -f %z "$WORK/n/bin/data/eval.dat")" = $NIKQUE_EVAL ] || { echo "nikque eval.dat is not of $NIKQUE_TAG"; exit 1; }
[ "$(stat -f %z "$WORK/e/bin/data/eval.dat")" = $NEWEDAX_EVAL ] || { echo "newedax eval.dat is not of $NEWEDAX_TAG"; exit 1; }

# newedax for macOS from the source of the tag (as the macOS jobs of its release workflow; 64 slots, its default)
for arch in arm64 x64-modern; do
	cc=arm64
	[ $arch = x64-modern ] && cc=x86_64
	rm -rf "$WORK/ne-$arch"
	cp -R "$NESRC" "$WORK/ne-$arch"
	mkdir -p "$WORK/ne-$arch/bin"
	make -s -C "$WORK/ne-$arch/src" build ARCH=$arch COMP=clang OS=osx CC="clang -arch $cc" BOOST_DIR="$BOOST_DIR" > "$WORK/ne-$arch.log" 2>&1 ||
		{ tail -30 "$WORK/ne-$arch.log"; exit 1; }
	[ -x "$WORK/ne-$arch/bin/mEdax-$arch" ] || { echo "no mEdax-$arch"; exit 1; }
done

mkdir -p "$DST/lang" "$DST/engines"
cp -R "$APP" "$DST/"
cp readme.txt LICENSE.txt "$DST/"
cp res/opening.ini "$DST/opening.ini"
cp lang/*.txt "$DST/lang/"
cp -R manual "$DST/"
# config.ini: the notes with the names of macOS
sed -e 's|^; dir= が空.*|; dir= が空：nikque GUI.app のあるフォルダの engines/nikque・engines/newedax。 / dir= empty: engines/nikque, engines/newedax next to nikque GUI.app.|' \
	-e 's|^; exe=auto.*|; exe=auto：Mac に合わせて mEdax-arm64（Apple シリコン）・mEdax-x64-modern（Intel）から選ぶ。 / exe=auto: chosen for the Mac.|' \
	-e 's|^; book= が空.*|; book= が空：エンジンのフォルダの data/book.dat。 / book= empty: data/book.dat of the engine folder.|' \
	res/default-config.ini > "$DST/config.ini"

source_txt() {  # <title> <repo> <tag> <commit> <how>
	printf '%s\n' "$1 $3" "" \
		"This engine is free software under the GNU General Public License version 3 (LICENSE.txt)." \
		"It runs as a separate program; nikque GUI (MIT License) starts it and talks to it with text commands." \
		"このエンジンは GNU GPL v3（LICENSE.txt）の自由なソフトウェアです。別のプログラムとして動き、" \
		"nikque GUI（MIT）が起動して文字の命令でやりとりします。" "" \
		"Source / ソース: $2" "Tag / タグ: $3" "Commit: $4" \
		"Files / ファイル: mEdax-arm64 (Apple silicon), mEdax-x64-modern (Intel), data/eval.dat" "$5"
}
E=$DST/engines/nikque
mkdir -p "$E/data"
cp "$WORK/n/bin/mEdax-arm64" "$WORK/n/bin/mEdax-x64-modern" "$E/"
cp "$WORK/n/bin/data/eval.dat" "$E/data/"
cp "$WORK/n/LICENSE" "$E/LICENSE.txt"
source_txt "Edax (C version, nikque)" https://github.com/Nikque/edax-reversi-AVX $NIKQUE_TAG $NIKQUE_COMMIT \
	"The files of the release ZIP of the tag. / タグの Release の ZIP のファイルです。" > "$E/SOURCE.txt"
for f in mEdax-arm64 mEdax-x64-modern data/eval.dat; do cmp "$WORK/n/bin/$f" "$E/$f"; done

E=$DST/engines/newedax
mkdir -p "$E/data"
cp "$WORK/ne-arm64/bin/mEdax-arm64" "$WORK/ne-x64-modern/bin/mEdax-x64-modern" "$E/"
cp "$WORK/e/bin/data/eval.dat" "$E/data/"
cp "$WORK/e/LICENSE" "$E/LICENSE.txt"
source_txt "newedax (C++ version of Edax)" https://github.com/Nikque/newedax $NEWEDAX_TAG $NEWEDAX_COMMIT \
	"The programs are built from the source of the tag (64 slots) for this distribution; data/eval.dat is of the release ZIP of the tag. / プログラムはこの配布物のためにタグのソース（64 枠）から作りました。data/eval.dat はタグの Release の ZIP のものです。" > "$E/SOURCE.txt"
cmp "$WORK/e/bin/data/eval.dat" "$E/data/eval.dat"
chmod +x "$DST"/engines/*/mEdax-*

# the engines run: the first problems of FFO solved by each (on this Mac's own kind of CPU)
for e in nikque newedax; do
	(cd "$DST/engines/$e" && ./mEdax-arm64 -n 1 -l 60 -solve "$NESRC/problem/fforum-1-19.obf" > "$WORK/solve-$e.log" 2>&1) ||
		{ cat "$WORK/solve-$e.log"; exit 1; }
	tail -2 "$WORK/solve-$e.log"
	rm -f "$DST/engines/$e/data/book.dat"  # (made by the engine when there is none)
done
lipo -info "$DST/nikque GUI.app/Contents/MacOS/nikque GUI"
for f in "$DST"/engines/*/mEdax-*; do lipo -info "$f"; done

# nothing that is made by a run
bad=$(find "$DST" \( -name book.dat -o -name kept.txt -o -name '*.log' -o -name '.DS_Store' \) -print)
[ -z "$bad" ] || { echo "files that must not be in the distribution: $bad"; exit 1; }

(cd dist && ditto -c -k --keepParent "$NAME" "$NAME.zip")
(cd "$DST" && find . -type f | sort | while read -r f; do printf '%s  %12s  %s\n' "$(shasum -a 256 "$f" | cut -d' ' -f1)" "$(stat -f %z "$f")" "${f#./}"; done) > "dist/$NAME-files.txt"
shasum -a 256 "dist/$NAME.zip" | tee -a "dist/$NAME-files.txt"
ls -la dist
