#!/bin/bash
# The Linux distribution of nikque GUI (run by .github/workflows/linux.yml after the build):
#   dist/nikque-gui-<version>-linux/  and  dist/nikque-gui-<version>-linux.tar.gz
# It holds nikque-gui (x86-64, GTK 3), config.ini (res/default-config.ini with the names of Linux in its notes),
# opening.ini, readme.txt, LICENSE.txt, lang/, manual/, and the two engines in engines/nikque and engines/newedax:
# lEdax-x86-64-v4, lEdax-x86-64-v3, lEdax-x86-64, data/eval.dat, LICENSE.txt, SOURCE.txt.
# The engines are built here from the sources of the tags of their releases (checked out by the workflow at those tags;
# the commits are checked), with data/eval.dat of the release ZIPs (checked by their SHA-256 and size):
#   nikque: the Linux programs of its release ZIP ask for a newer C library (GLIBC 2.38) than Ubuntu 22.04 has.
#   newedax: its release ZIP has no engine for Linux.
# usage: BOOST_DIR=<Boost 1.92> CXX=<g++> tools/make_dist_linux.sh <nikque-gui> <nikque zip> <newedax zip> <newedax source> <nikque source>
set -euo pipefail

EXE=$1 NZIP=$2 EZIP=$3 NESRC=$(cd "$4" && pwd) NKSRC=$(cd "$5" && pwd)
[ -d "${BOOST_DIR:-}/boost" ] || { echo "BOOST_DIR: the folder of Boost 1.92 (its headers) for newedax"; exit 1; }
CXX=${CXX:-g++}
CC=${CXX/g++/gcc}
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
ARCHS="x86-64-v4 x86-64-v3 x86-64"

VER=$(sed -n 's/^project(nikque_gui VERSION \([0-9.]*\) .*/\1/p' CMakeLists.txt)
[ -n "$VER" ] || { echo "no version in CMakeLists.txt"; exit 1; }
NAME=nikque-gui-$VER-linux
DST=dist/$NAME
[ ! -e "$DST" ] || { echo "$DST is there"; exit 1; }

check_zip() {  # <zip> <sha256> <size>
	local h s
	h=$(sha256sum "$1" | cut -d' ' -f1)
	s=$(stat -c %s "$1")
	[ "$h" = "$2" ] && [ "$s" = "$3" ] || { echo "$1: SHA-256 $h, $s bytes (not the values of the release)"; exit 1; }
	echo "zip ok  $h  $s  $1"
}
check_zip "$NZIP" $NIKQUE_SHA $NIKQUE_SIZE
check_zip "$EZIP" $NEWEDAX_SHA $NEWEDAX_SIZE
[ "$(git -C "$NESRC" rev-parse HEAD)" = $NEWEDAX_COMMIT ] || { echo "$NESRC is not $NEWEDAX_TAG ($NEWEDAX_COMMIT)"; exit 1; }
[ "$(git -C "$NKSRC" rev-parse HEAD)" = $NIKQUE_COMMIT ] || { echo "$NKSRC is not $NIKQUE_TAG ($NIKQUE_COMMIT)"; exit 1; }

WORK=$(mktemp -d)
unzip -q "$NZIP" -d "$WORK/n"
unzip -q "$EZIP" -d "$WORK/e"
[ "$(stat -c %s "$WORK/n/bin/data/eval.dat")" = $NIKQUE_EVAL ] || { echo "nikque eval.dat is not of $NIKQUE_TAG"; exit 1; }
[ "$(stat -c %s "$WORK/e/bin/data/eval.dat")" = $NEWEDAX_EVAL ] || { echo "newedax eval.dat is not of $NEWEDAX_TAG"; exit 1; }

# the engines for Linux from the sources of the tags (as the Linux jobs of their build workflows; newedax: 64 slots, its
# default). The libraries of the compiler are in the programs: they run where the system has older ones.
"$CC" --version | head -1
"$CXX" --version | head -1
for a in $ARCHS; do
	rm -rf "$WORK/nk-$a"
	cp -R "$NKSRC" "$WORK/nk-$a"
	mkdir -p "$WORK/nk-$a/bin"
	make -s -C "$WORK/nk-$a/src" build ARCH=$a COMP=gcc OS=linux CC="$CC -static-libgcc" 		> "$WORK/nk-$a.log" 2>&1 || { tail -40 "$WORK/nk-$a.log"; exit 1; }
	[ -x "$WORK/nk-$a/bin/lEdax-$a" ] || { echo "no lEdax-$a (nikque)"; tail -40 "$WORK/nk-$a.log"; exit 1; }
done
for a in $ARCHS; do
	rm -rf "$WORK/ne-$a"
	cp -R "$NESRC" "$WORK/ne-$a"
	mkdir -p "$WORK/ne-$a/bin"
	make -s -C "$WORK/ne-$a/src" build ARCH=$a COMP=gcc OS=linux CC="$CXX -static-libstdc++ -static-libgcc" BOOST_DIR="$BOOST_DIR" \
		> "$WORK/ne-$a.log" 2>&1 || { tail -40 "$WORK/ne-$a.log"; exit 1; }
	[ -x "$WORK/ne-$a/bin/lEdax-$a" ] || { echo "no lEdax-$a"; tail -40 "$WORK/ne-$a.log"; exit 1; }
done

mkdir -p "$DST/lang" "$DST/engines"
cp "$EXE" "$DST/nikque-gui"
chmod +x "$DST/nikque-gui"
cp readme.txt LICENSE.txt "$DST/"
cp res/opening.ini "$DST/opening.ini"
cp lang/*.txt "$DST/lang/"
cp -R manual "$DST/"
# config.ini: the notes with the names of Linux
sed -e 's|^; dir= が空.*|; dir= が空：nikque-gui のあるフォルダの engines/nikque・engines/newedax。 / dir= empty: engines/nikque, engines/newedax next to nikque-gui.|' \
	-e 's|^; exe=auto.*|; exe=auto：CPU に合わせて lEdax-x86-64-v4・-v3・lEdax-x86-64 から選ぶ。 / exe=auto: chosen for the CPU.|' \
	-e 's|^; book= が空.*|; book= が空：エンジンのフォルダの data/book.dat。 / book= empty: data/book.dat of the engine folder.|' \
	res/default-config.ini > "$DST/config.ini"

source_txt() {  # <title> <repo> <tag> <commit> <how>
	printf '%s\n' "$1 $3" "" \
		"This engine is free software under the GNU General Public License version 3 (LICENSE.txt)." \
		"It runs as a separate program; nikque GUI (MIT License) starts it and talks to it with text commands." \
		"このエンジンは GNU GPL v3（LICENSE.txt）の自由なソフトウェアです。別のプログラムとして動き、" \
		"nikque GUI（MIT）が起動して文字の命令でやりとりします。" "" \
		"Source / ソース: $2" "Tag / タグ: $3" "Commit: $4" \
		"Files / ファイル: lEdax-x86-64-v4 (AVX-512), lEdax-x86-64-v3 (AVX2), lEdax-x86-64, data/eval.dat" "$5"
}
E=$DST/engines/nikque
mkdir -p "$E/data"
for a in $ARCHS; do cp "$WORK/nk-$a/bin/lEdax-$a" "$E/"; done
cp "$WORK/n/bin/data/eval.dat" "$E/data/"
cp "$WORK/n/LICENSE" "$E/LICENSE.txt"
source_txt "Edax (C version, nikque)" https://github.com/Nikque/edax-reversi-AVX $NIKQUE_TAG $NIKQUE_COMMIT \
	"The programs are built from the source of the tag for this distribution (the Linux programs of the release ZIP ask for GLIBC 2.38); data/eval.dat is of the release ZIP of the tag. / プログラムはこの配布物のためにタグのソースから作りました（Release の ZIP の Linux 用は GLIBC 2.38 を求めます）。data/eval.dat はタグの Release の ZIP のものです。" > "$E/SOURCE.txt"
cmp "$WORK/n/bin/data/eval.dat" "$E/data/eval.dat"

E=$DST/engines/newedax
mkdir -p "$E/data"
for a in $ARCHS; do cp "$WORK/ne-$a/bin/lEdax-$a" "$E/"; done
cp "$WORK/e/bin/data/eval.dat" "$E/data/"
cp "$WORK/e/LICENSE" "$E/LICENSE.txt"
source_txt "newedax (C++ version of Edax)" https://github.com/Nikque/newedax $NEWEDAX_TAG $NEWEDAX_COMMIT \
	"The programs are built from the source of the tag (64 slots) for this distribution; data/eval.dat is of the release ZIP of the tag. / プログラムはこの配布物のためにタグのソース（64 枠）から作りました。data/eval.dat はタグの Release の ZIP のものです。" > "$E/SOURCE.txt"
cmp "$WORK/e/bin/data/eval.dat" "$E/data/eval.dat"
chmod +x "$DST"/engines/*/lEdax-*

# the engines run: the first problems of FFO solved by each program this computer's CPU can run
flags=$(grep -m1 '^flags' /proc/cpuinfo || true)
for e in nikque newedax; do
	for a in $ARCHS; do
		case $a in
			x86-64-v4) echo "$flags" | grep -qw avx512vl || { echo "$e lEdax-$a: not run (no AVX-512 on this computer)"; continue; } ;;
			x86-64-v3) echo "$flags" | grep -qw avx2 || { echo "$e lEdax-$a: not run (no AVX2 on this computer)"; continue; } ;;
		esac
		(cd "$DST/engines/$e" && "./lEdax-$a" -n 1 -l 60 -solve "$NESRC/problem/fforum-1-19.obf" > "$WORK/solve-$e-$a.log" 2>&1) ||
			{ cat "$WORK/solve-$e-$a.log"; exit 1; }
		echo "$e lEdax-$a:"
		tail -2 "$WORK/solve-$e-$a.log"
		rm -f "$DST/engines/$e/data/book.dat"  # (made by the engine when there is none)
	done
done

# what the programs ask of the system: the newest version of the C library they name, and the shared libraries
for f in "$DST/nikque-gui" "$DST"/engines/*/lEdax-*; do
	printf '%s: %s\n' "${f#"$DST"/}" "$(objdump -T "$f" 2>/dev/null | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)"
done
ldd "$DST/nikque-gui" | grep -c . | sed 's/^/nikque-gui: shared libraries: /'
if ldd "$DST/nikque-gui" | grep -q 'libstdc++'; then echo "nikque-gui asks for libstdc++ of the system"; exit 1; fi

# nothing that is made by a run
bad=$(find "$DST" \( -name book.dat -o -name kept.txt -o -name '*.log' \) -print)
[ -z "$bad" ] || { echo "files that must not be in the distribution: $bad"; exit 1; }

tar -C dist --sort=name --owner=0 --group=0 --numeric-owner -czf "dist/$NAME.tar.gz" "$NAME"
(cd "$DST" && find . -type f | sort | while read -r f; do printf '%s  %12s  %s\n' "$(sha256sum "$f" | cut -d' ' -f1)" "$(stat -c %s "$f")" "${f#./}"; done) > "dist/$NAME-files.txt"
sha256sum "dist/$NAME.tar.gz" | tee -a "dist/$NAME-files.txt"
ls -la dist
