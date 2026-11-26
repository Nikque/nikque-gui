#!/bin/bash
# The Android app of nikque GUI (run by .github/workflows/android.yml):
#   dist/nikque-gui-<version>-android.apk       for devices (arm64-v8a, Android 10 or later), signed with the key given
#   dist/nikque-gui-<version>-android-ci.apk    for the emulator of the CI (x86_64, debug; it runs a script at its start)
# The APK holds the screen (Java), libnikquegui.so (the C++ of the GUI), the files of the app as assets (config.ini,
# opening.ini, readme.txt, LICENSE.txt, lang/, and for each engine data/eval.dat, LICENSE.txt, SOURCE.txt) and the two
# engines as programs among its native libraries (libedax_nikque.so, libedax_newedax.so: started as child processes).
# Both ABIs use the pinned release source, NDK and an execution profile collected for that ABI.
# The pinned release ZIPs provide only the evaluation weights and license files.
# usage: BOOST_DIR=<Boost 1.92> ANDROID_NDK=<NDK> tools/make_dist_android.sh <nikque zip> <newedax zip> <newedax source> <nikque source> <script of the CI>
# The key of the release APK: NG_KEYSTORE, NG_KEYSTORE_PASSWORD, NG_KEY_ALIAS, NG_KEY_PASSWORD (android/app/build.gradle).
set -euo pipefail

NZIP=$(realpath "$1") EZIP=$(realpath "$2") NESRC=$(cd "$3" && pwd) NKSRC=$(cd "$4" && pwd) SMOKE=$(realpath "$5")
[ -d "${BOOST_DIR:-}/boost" ] || { echo "BOOST_DIR: the folder of Boost 1.92 (its headers) for newedax"; exit 1; }
[ -x "${ANDROID_NDK:-}/ndk-build" ] || { echo "ANDROID_NDK: the folder of the NDK"; exit 1; }
[ -f "${NG_KEYSTORE:-}" ] || { echo "NG_KEYSTORE: the keystore the release APK is signed with"; exit 1; }
NIKQUE_TAG=v4.5.5-nikque.34
NIKQUE_WEIGHT_SHA=f8b2299612d9fa4414157e70e932636e33111c2602d0c2fc382a7d90ef21b792
NIKQUE_LICENSE_SHA=0b383d5a63da644f628d99c33976ea6487ed89aaa59f0b3257992deac1171e6b
NIKQUE_COMMIT=4d16210cc91bb99fddaa372dceef8e2ca8990bfc
NIKQUE_EVAL=13952436
NEWEDAX_TAG=v4.5.5-heath.21
NEWEDAX_WEIGHT_SHA=6f8ceb1f9d8e8fcfc9c5117ace2b31f5f893ea54336d44e94fd74ee9e9bf1dbc
NEWEDAX_LICENSE_SHA=8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903
NEWEDAX_COMMIT=87560a37e2a3aef4f9d58303b2aeec39b8f7ccc9
NEWEDAX_EVAL=107612578

VER=$(sed -n 's/^project(nikque_gui VERSION \([0-9.]*\) .*/\1/p' CMakeLists.txt)
[ -n "$VER" ] || { echo "no version in CMakeLists.txt"; exit 1; }
NAME=nikque-gui-$VER-android
[ ! -e "dist/$NAME.apk" ] || { echo "dist/$NAME.apk is there"; exit 1; }

[ "$(git -C "$NESRC" rev-parse HEAD)" = $NEWEDAX_COMMIT ] || { echo "$NESRC is not $NEWEDAX_TAG ($NEWEDAX_COMMIT)"; exit 1; }
[ "$(git -C "$NKSRC" rev-parse HEAD)" = $NIKQUE_COMMIT ] || { echo "$NKSRC is not $NIKQUE_TAG ($NIKQUE_COMMIT)"; exit 1; }
# the sources of the engines as committed (changes not committed and files of an earlier build are not taken)
export_source() {  # <git folder> <commit> <folder>
	mkdir -p "$3"
	git -C "$1" archive --format=tar "$2" src include problem tests/check-solve.py | tar -x -C "$3"
}
# the files of the GUI as committed: no changes not committed, no files not committed among those copied
dirty=$({ git status --porcelain --untracked-files=no; git status --porcelain -- readme.txt LICENSE.txt res lang; } | sort -u)
[ -z "$dirty" ] || { echo "$dirty"; echo "the files of the GUI are not those of the commit"; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf -- "$WORK"' EXIT
mkdir -p "$WORK/n/bin/data" "$WORK/e/bin/data"
unzip -p "$NZIP" bin/data/eval.dat > "$WORK/n/bin/data/eval.dat"
unzip -p "$NZIP" LICENSE > "$WORK/n/LICENSE"
unzip -p "$EZIP" bin/data/eval.dat > "$WORK/e/bin/data/eval.dat"
unzip -p "$EZIP" LICENSE > "$WORK/e/LICENSE"
[ "$(stat -c %s "$WORK/n/bin/data/eval.dat")" = $NIKQUE_EVAL ] || { echo "nikque eval.dat is not of $NIKQUE_TAG"; exit 1; }
[ "$(stat -c %s "$WORK/e/bin/data/eval.dat")" = $NEWEDAX_EVAL ] || { echo "newedax eval.dat is not of $NEWEDAX_TAG"; exit 1; }

# The APK uses only these assets. Verify each byte without depending on the rest of either ZIP.
printf '%s  %s\n' "$NIKQUE_WEIGHT_SHA" "$WORK/n/bin/data/eval.dat" \
    "$NIKQUE_LICENSE_SHA" "$WORK/n/LICENSE" "$NEWEDAX_WEIGHT_SHA" "$WORK/e/bin/data/eval.dat" \
    "$NEWEDAX_LICENSE_SHA" "$WORK/e/LICENSE" | sha256sum -c

# Build each engine for each ABI with its own measured Android profile.
LIBS=$WORK/jnilibs
export_source "$NKSRC" $NIKQUE_COMMIT "$WORK/nk"
mkdir -p "$WORK/nesrc/third_party"
export_source "$NESRC" $NEWEDAX_COMMIT "$WORK/nesrc/ne"
ln -s "$BOOST_DIR" "$WORK/nesrc/third_party/boost_1_92_0"
for abi in arm64-v8a x86_64; do
    mkdir -p "$LIBS/$abi"
    bash tools/android_pgo.sh "$abi" "$WORK/nk" "$WORK/n/bin/data/eval.dat" \
        "$LIBS/$abi/libedax_nikque.so" "dist/android-pgo/$abi/nikque"
    bash tools/android_pgo.sh "$abi" "$WORK/nesrc/ne" "$WORK/e/bin/data/eval.dat" \
        "$LIBS/$abi/libedax_newedax.so" "dist/android-pgo/$abi/heath"
done

# ---- the files of the app (the assets)
A=$WORK/assets/app
mkdir -p "$A/lang"
cp readme.txt LICENSE.txt "$A/"
cp res/opening.ini "$A/opening.ini"
cp lang/*.txt "$A/lang/"
sed -e 's|^; dir= が空.*|; dir= が空：アプリのファイルの engines/nikque・engines/newedax。 / dir= empty: engines/nikque, engines/newedax in the files of the app.|' \
	-e 's|^; exe=auto.*|; exe=auto：アプリに入っているエンジン（aEdax）。 / exe=auto: the engine in the app (aEdax).|' \
	-e 's|^; book= が空.*|; book= が空：エンジンのフォルダの data/book.dat。 / book= empty: data/book.dat of the engine folder.|' \
	res/default-config.ini > "$A/config.ini"
# (a small screen: the row of the bookmarks is hidden at first and the graph is lower, for more rows of the table)
grep -q '^\[window\]' "$A/config.ini" && { echo "config.ini has [window] already"; exit 1; }
printf '%s\n' '[window]' 'bookmarks=0' 'graph=24' >> "$A/config.ini"

source_txt() {  # <title> <repo> <tag> <commit> <how>
	printf '%s\n' "$1 $3" "" \
		"This engine is free software under the GNU General Public License version 3 (LICENSE.txt)." \
		"It runs as a separate program; nikque GUI (MIT License) starts it and talks to it with text commands." \
		"このエンジンは GNU GPL v3（LICENSE.txt）の自由なソフトウェアです。別のプログラムとして動き、" \
		"nikque GUI（MIT）が起動して文字の命令でやりとりします。" "" \
		"Source / ソース: $2" "Tag / タグ: $3" "Commit: $4" \
		"Files / ファイル: Android aEdax (in the app as a native library), data/eval.dat" "$5"
}
mkdir -p "$A/engines/nikque/data" "$A/engines/newedax/data"
cp "$WORK/n/bin/data/eval.dat" "$A/engines/nikque/data/"
cp "$WORK/n/LICENSE" "$A/engines/nikque/LICENSE.txt"
source_txt "Edax (C version, nikque)" https://github.com/Nikque/edax-reversi-AVX $NIKQUE_TAG $NIKQUE_COMMIT \
	"The executable uses Android PGO from the tagged source. The weight is from the release ZIP. / 実行ファイルはタグのソースから PGO でビルドしました。評価重みは Release の ZIP から取得しました。" > "$A/engines/nikque/SOURCE.txt"
cp "$WORK/e/bin/data/eval.dat" "$A/engines/newedax/data/"
cp "$WORK/e/LICENSE" "$A/engines/newedax/LICENSE.txt"
source_txt "Heath (formerly newedax, C++ version of Edax)" https://github.com/Nikque/Heath $NEWEDAX_TAG $NEWEDAX_COMMIT \
	"The executable uses Android PGO from the tagged source. The 64-slot weight is from the release ZIP. / 実行ファイルはタグのソースから PGO でビルドしました。64 枠の評価重みは Release の ZIP から取得しました。" > "$A/engines/newedax/SOURCE.txt"

# the assets of the APK of the CI: the same, with the script run at the start (a lower level: the emulator is slow)
cp -R "$WORK/assets" "$WORK/assets-ci"
cp "$SMOKE" "$WORK/assets-ci/app/smoke.txt"
sed -i -e 's/^startup=.*/startup=smoke.txt/' -e 's/^level=21/level=8/' "$WORK/assets-ci/app/config.ini"
grep -n "^startup\|^level\|^kind" "$WORK/assets-ci/app/config.ini"

# ---- the APKs
mkdir -p dist
rm -rf android/app/build android/app/.cxx  # (nothing of an earlier build)
gradle --version | grep -E "^Gradle|JVM" || true
gradle -p android --no-daemon -q assembleRelease -PngAbi=arm64-v8a -PngAssets="$WORK/assets" -PngJniLibs="$LIBS"
cp android/app/build/outputs/apk/release/app-release.apk "dist/$NAME.apk"
gradle -p android --no-daemon -q assembleDebug -PngAbi=x86_64 -PngAssets="$WORK/assets-ci" -PngJniLibs="$LIBS"
cp android/app/build/outputs/apk/debug/app-debug.apk "dist/$NAME-ci.apk"

# what is in the release APK, and its signature
BT=$(ls -d "${ANDROID_HOME:-$ANDROID_SDK_ROOT}"/build-tools/* | sort -V | tail -1)
"$BT/apksigner" verify --print-certs "dist/$NAME.apk" | head -4
"$BT/aapt2" dump badging "dist/$NAME.apk" | grep -E "^package|sdkVersion|native-code" || true
unzip -l "dist/$NAME.apk" | grep -E "lib/|assets/app/(config.ini|engines/.*/eval.dat)|classes"
if unzip -l "dist/$NAME.apk" | grep -q "lib/x86_64/"; then echo "the release APK holds x86_64"; exit 1; fi
# the programs of each APK for its CPU: the GUI and the two engines
for f in libnikquegui.so libedax_nikque.so libedax_newedax.so; do
	unzip -l "dist/$NAME.apk" | grep -q "lib/arm64-v8a/$f" || { echo "the release APK has no lib/arm64-v8a/$f"; exit 1; }
	unzip -l "dist/$NAME-ci.apk" | grep -q "lib/x86_64/$f" || { echo "the APK of the CI has no lib/x86_64/$f"; exit 1; }
done
for label in nikque newedax; do
    for abi in arm64-v8a x86_64; do
        apk="dist/$NAME.apk"
        [ "$abi" != x86_64 ] || apk="dist/$NAME-ci.apk"
        unzip -p "$apk" "lib/$abi/libedax_$label.so" | cmp - "$LIBS/$abi/libedax_$label.so"
    done
done
unzip -l "dist/$NAME.apk" | awk 'NR>3 && $4!="" {printf "%12s  %s\n", $1, $4}' | sort -k2 > "dist/$NAME-files.txt"
sha256sum "dist/$NAME.apk" | tee -a "dist/$NAME-files.txt"
ls -la dist
