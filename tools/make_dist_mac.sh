#!/bin/bash
# The macOS distribution of nikque GUI (run by .github/workflows/mac.yml after the build):
#   dist/nikque-gui-<version>-mac/  and  dist/nikque-gui-<version>-mac.zip
# It holds "nikque GUI.app" (Apple silicon and Intel), config.ini (res/default-config.ini with the names of macOS in its
# notes), opening.ini, readme.txt, LICENSE.txt, lang/, manual/, and the two engines in engines/nikque and
# engines/newedax: mEdax-arm64 (Apple silicon), mEdax-x64-modern (Intel), data/eval.dat, LICENSE.txt, SOURCE.txt.
# Both engines use native PGO builds from their pinned source tags.
# usage: tools/make_dist_mac.sh <app> <nikque zip> <Heath zip>  # requires PGO_MAC_DIR
#        BOOST_DIR=... tools/make_dist_mac.sh --train <nikque zip> <Heath zip> <Heath src> <nikque src> <output>
set -euo pipefail

APP=$1 NZIP=$2 EZIP=$3
NIKQUE_TAG=v4.5.5-nikque.34
NIKQUE_SHA=ea8ce8f654283a1030f66dd5f170a9960f7fc73e5f663742a1547ee122261a7a
NIKQUE_SIZE=19275144
NIKQUE_COMMIT=4d16210cc91bb99fddaa372dceef8e2ca8990bfc
NIKQUE_EVAL=13952436
NEWEDAX_TAG=v4.5.5-heath.21
NEWEDAX_SHA=edc50601c9cd7e37c1220ac1fcfa50b532515cf238af5462a1cf7e09235766e5
NEWEDAX_SIZE=68972596
NEWEDAX_COMMIT=87560a37e2a3aef4f9d58303b2aeec39b8f7ccc9
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

WORK=$(mktemp -d)

unzip -q "$NZIP" -d "$WORK/n"
unzip -q "$EZIP" -d "$WORK/e"
[ "$(stat -f %z "$WORK/n/bin/data/eval.dat")" = $NIKQUE_EVAL ] || { echo "nikque eval.dat is not of $NIKQUE_TAG"; exit 1; }
[ "$(stat -f %z "$WORK/e/bin/data/eval.dat")" = $NEWEDAX_EVAL ] || { echo "newedax eval.dat is not of $NEWEDAX_TAG"; exit 1; }

if [ "$APP" = --train ]; then
    NESRC=$(cd "$4" && pwd)
    NKSRC=$(cd "$5" && pwd)
    mkdir -p "$6"
    out=$(cd "$6" && pwd)
    [ -d "${BOOST_DIR:-}/boost" ]
    [ "$(git -C "$NESRC" rev-parse HEAD)" = "$NEWEDAX_COMMIT" ]
    [ "$(git -C "$NKSRC" rev-parse HEAD)" = "$NIKQUE_COMMIT" ]
    cpu=$(uname -m)
    case $cpu in
        arm64) arch=arm64 ;;
        x86_64) arch=x64-modern ;;
        *) echo "Unsupported native macOS architecture: $cpu" >&2; exit 1 ;;
    esac
    for family in nikque newedax; do
        case $family in
            nikque) tree=$NKSRC commit=$NIKQUE_COMMIT eval_file=$WORK/n/bin/data/eval.dat compiler=clang ;;
            newedax) tree=$NESRC commit=$NEWEDAX_COMMIT eval_file=$WORK/e/bin/data/eval.dat compiler=clang++ ;;
        esac
        stage=$WORK/$family
        mkdir -p "$stage"
        git -C "$tree" archive --format=tar "$commit" | tar -x -C "$stage"
        evidence=$out/$family/$arch
        bash tools/train_engine_pgo.sh "$stage" "mEdax-$arch" "$eval_file" "$stage/problem/fforum-1-19.obf" "$evidence" \
            ARCH="$arch" COMP=clang OS=osx CC="$compiler -arch $cpu" BOOST_DIR="$BOOST_DIR"
        printf '%s\n' "$commit" > "$evidence/source-commit.txt"
        lipo "$stage/bin/mEdax-$arch" -verify_arch "$cpu"
        cp "$stage/bin/mEdax-$arch" "$evidence/"
        cp "$stage/problem/fforum-1-19.obf" "$evidence/"
    done
    exit 0
fi

dirty=$({ git status --porcelain --untracked-files=no; git status --porcelain -- readme.txt LICENSE.txt res lang manual; } | sort -u)
[ -z "$dirty" ] || { echo "$dirty"; echo "the files of the GUI are not those of the commit"; exit 1; }
[ "$(stat -f %m "$APP/Contents/MacOS/nikque GUI")" -ge "$(git log -1 --format=%ct)" ]
: "${PGO_MAC_DIR:?PGO_MAC_DIR must contain evidence and binaries from both native training jobs}"
PGO_MAC_DIR=$(cd "$PGO_MAC_DIR" && pwd)

copy_engine() {
    local family=$1 commit=$2 arch cpu src
    for arch in arm64 x64-modern; do
        src=$PGO_MAC_DIR/$family/$arch
        [ "$(cat "$src/source-commit.txt")" = "$commit" ]
        [ -s "$src/profile-sha256.txt" ]
        grep -Fq '19 positions; 0 erroneous move; 0 erroneous score;' "$src/final-solve.txt"
        cp "$src/mEdax-$arch" "$E/"
        (cd "$E" && shasum -a 256 -c "$src/binary.sha256")
        cpu=$arch
        [ "$arch" != x64-modern ] || cpu=x86_64
        lipo "$E/mEdax-$arch" -verify_arch "$cpu"
    done
}

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
copy_engine nikque "$NIKQUE_COMMIT"
cp "$WORK/n/bin/data/eval.dat" "$E/data/"
cp "$WORK/n/LICENSE" "$E/LICENSE.txt"
source_txt "Edax (C version, nikque)" https://github.com/Nikque/edax-reversi-AVX $NIKQUE_TAG $NIKQUE_COMMIT \
	"The programs use native PGO builds from the tagged source. data/eval.dat is from the release ZIP. / 実行ファイルはタグのソースから PGO でビルドしました。data/eval.dat は Release の ZIP から収録しました。" > "$E/SOURCE.txt"
cmp "$WORK/n/bin/data/eval.dat" "$E/data/eval.dat"

E=$DST/engines/newedax
mkdir -p "$E/data"
copy_engine newedax "$NEWEDAX_COMMIT"
cp "$WORK/e/bin/data/eval.dat" "$E/data/"
cp "$WORK/e/LICENSE" "$E/LICENSE.txt"
source_txt "Heath (formerly newedax, C++ version of Edax)" https://github.com/Nikque/Heath $NEWEDAX_TAG $NEWEDAX_COMMIT \
	"The programs use native PGO builds from the tagged source (64 slots); data/eval.dat is of the release ZIP of the tag. / 実行ファイルはタグのソース（64 枠）から PGO でビルドしました。data/eval.dat はタグの Release の ZIP のものです。" > "$E/SOURCE.txt"
cmp "$WORK/e/bin/data/eval.dat" "$E/data/eval.dat"
chmod +x "$DST"/engines/*/mEdax-*

# the engines run: the first problems of FFO solved by each (on this Mac's own kind of CPU)
case "$(uname -m)" in
	arm64) host_engine=mEdax-arm64; host_arch=arm64 ;;
	x86_64) host_engine=mEdax-x64-modern; host_arch=x64-modern ;;
	*) echo "Unsupported Mac architecture"; exit 1 ;;
esac
for e in nikque newedax; do
	lipo "$DST/engines/$e/mEdax-arm64" -verify_arch arm64
	lipo "$DST/engines/$e/mEdax-x64-modern" -verify_arch x86_64
	(cd "$DST/engines/$e" && "./$host_engine" -n 1 -l 60 -book-usage off -solve "$PGO_MAC_DIR/$e/$host_arch/fforum-1-19.obf" > "$WORK/solve-$e.log" 2>&1) ||
		{ cat "$WORK/solve-$e.log"; exit 1; }
	tail -2 "$WORK/solve-$e.log"
	grep -Fq '19 positions; 0 erroneous move; 0 erroneous score;' "$WORK/solve-$e.log"
	rm -f "$DST/engines/$e/data/book.dat"  # (made by the engine when there is none)
done
lipo -info "$DST/nikque GUI.app/Contents/MacOS/nikque GUI"
for f in "$DST"/engines/*/mEdax-*; do lipo -info "$f"; done
# the GUI for both kinds of Mac: a build with one of them is not a distribution
for arch in arm64 x86_64; do
	lipo "$DST/nikque GUI.app/Contents/MacOS/nikque GUI" -verify_arch $arch || { echo "the GUI has no $arch"; exit 1; }
done

# nothing that is made by a run
bad=$(find "$DST" \( -name book.dat -o -name kept.txt -o -name '*.log' -o -name '.DS_Store' \) -print)
[ -z "$bad" ] || { echo "files that must not be in the distribution: $bad"; exit 1; }

(cd dist && ditto -c -k --keepParent "$NAME" "$NAME.zip")
(cd "$DST" && find . -type f | sort | while read -r f; do printf '%s  %12s  %s\n' "$(shasum -a 256 "$f" | cut -d' ' -f1)" "$(stat -f %z "$f")" "${f#./}"; done) > "dist/$NAME-files.txt"
shasum -a 256 "dist/$NAME.zip" | tee -a "dist/$NAME-files.txt"
ls -la dist
