#!/bin/bash
# The Android app of nikque GUI (run by .github/workflows/android.yml):
#   dist/nikque-gui-<version>-android.apk       for devices (arm64-v8a, Android 10 or later), signed with the key given
#   dist/nikque-gui-<version>-android-ci.apk    for the emulator of the CI (x86_64, debug; it runs a script at its start)
# The APK holds the screen (Java), libnikquegui.so (the C++ of the GUI), the files of the app as assets (config.ini,
# opening.ini, readme.txt, LICENSE.txt, lang/, and for each engine data/eval.dat, LICENSE.txt, SOURCE.txt) and the two
# engines as programs among its native libraries (libedax_nikque.so, libedax_newedax.so: started as child processes).
#   arm64-v8a: aEdax-arm64-v8a of the release ZIPs of the engines (checked by their SHA-256 and size), as they are.
#   x86_64 (the CI only): the release ZIPs have none: built here with the NDK from the sources of the tags of the
#           releases (checked out by the workflow at those tags; the commits are checked).
# usage: BOOST_DIR=<Boost 1.92> ANDROID_NDK=<NDK> tools/make_dist_android.sh <nikque zip> <newedax zip> <newedax source> <nikque source> <script of the CI>
# The key of the release APK: NG_KEYSTORE, NG_KEYSTORE_PASSWORD, NG_KEY_ALIAS, NG_KEY_PASSWORD (android/app/build.gradle).
set -euo pipefail

NZIP=$(realpath "$1") EZIP=$(realpath "$2") NESRC=$(cd "$3" && pwd) NKSRC=$(cd "$4" && pwd) SMOKE=$(realpath "$5")
[ -d "${BOOST_DIR:-}/boost" ] || { echo "BOOST_DIR: the folder of Boost 1.92 (its headers) for newedax"; exit 1; }
[ -x "${ANDROID_NDK:-}/ndk-build" ] || { echo "ANDROID_NDK: the folder of the NDK"; exit 1; }
[ -f "${NG_KEYSTORE:-}" ] || { echo "NG_KEYSTORE: the keystore the release APK is signed with"; exit 1; }
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
NAME=nikque-gui-$VER-android
[ ! -e "dist/$NAME.apk" ] || { echo "dist/$NAME.apk is there"; exit 1; }

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
# the sources of the engines as committed (changes not committed and files of an earlier build are not taken)
export_source() {  # <git folder> <commit> <folder>
	mkdir -p "$3"
	git -C "$1" archive --format=tar "$2" | tar -x -C "$3"
}
# the files of the GUI as committed: no changes not committed, no files not committed among those copied
dirty=$({ git status --porcelain --untracked-files=no; git status --porcelain -- readme.txt LICENSE.txt res lang; } | sort -u)
[ -z "$dirty" ] || { echo "$dirty"; echo "the files of the GUI are not those of the commit"; exit 1; }

WORK=$(mktemp -d)
unzip -q "$NZIP" -d "$WORK/n"
unzip -q "$EZIP" -d "$WORK/e"
[ "$(stat -c %s "$WORK/n/bin/data/eval.dat")" = $NIKQUE_EVAL ] || { echo "nikque eval.dat is not of $NIKQUE_TAG"; exit 1; }
[ "$(stat -c %s "$WORK/e/bin/data/eval.dat")" = $NEWEDAX_EVAL ] || { echo "newedax eval.dat is not of $NEWEDAX_TAG"; exit 1; }
for z in n e; do [ -f "$WORK/$z/bin/aEdax-arm64-v8a" ] || { echo "no aEdax-arm64-v8a in the ZIP ($z)"; exit 1; }; done

# ---- the engines as native libraries
LIBS=$WORK/jnilibs
mkdir -p "$LIBS/arm64-v8a" "$LIBS/x86_64"
cp "$WORK/n/bin/aEdax-arm64-v8a" "$LIBS/arm64-v8a/libedax_nikque.so"
cp "$WORK/e/bin/aEdax-arm64-v8a" "$LIBS/arm64-v8a/libedax_newedax.so"
file "$LIBS"/arm64-v8a/*

# x86_64 for the emulator, from the sources of the tags (newedax finds Boost in third_party next to its folder)
export_source "$NKSRC" $NIKQUE_COMMIT "$WORK/nk"
mkdir -p "$WORK/nesrc/third_party"
export_source "$NESRC" $NEWEDAX_COMMIT "$WORK/nesrc/ne"
ln -s "$BOOST_DIR" "$WORK/nesrc/third_party/boost_1_92_0"
for e in nikque newedax; do
	src=$WORK/nk/src
	[ $e = newedax ] && src=$WORK/nesrc/ne/src
	(cd "$src" && "$ANDROID_NDK/ndk-build" NDK_PROJECT_PATH=. NDK_APPLICATION_MK=./Application.mk APP_ABI=x86_64 APP_PLATFORM=android-29 -j4) \
		> "$WORK/ndk-$e.log" 2>&1 || { tail -60 "$WORK/ndk-$e.log"; exit 1; }
	[ -f "$src/libs/x86_64/aEdax" ] || { echo "no aEdax for x86_64 ($e)"; tail -60 "$WORK/ndk-$e.log"; exit 1; }
	cp "$src/libs/x86_64/aEdax" "$LIBS/x86_64/libedax_$e.so"
done
file "$LIBS"/x86_64/*

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
		"Files / ファイル: aEdax-arm64-v8a (in the app as a native library), data/eval.dat" "$5"
}
mkdir -p "$A/engines/nikque/data" "$A/engines/newedax/data"
cp "$WORK/n/bin/data/eval.dat" "$A/engines/nikque/data/"
cp "$WORK/n/LICENSE" "$A/engines/nikque/LICENSE.txt"
source_txt "Edax (C version, nikque)" https://github.com/Nikque/edax-reversi-AVX $NIKQUE_TAG $NIKQUE_COMMIT \
	"The files of the release ZIP of the tag. / タグの Release の ZIP のファイルです。" > "$A/engines/nikque/SOURCE.txt"
cp "$WORK/e/bin/data/eval.dat" "$A/engines/newedax/data/"
cp "$WORK/e/LICENSE" "$A/engines/newedax/LICENSE.txt"
source_txt "Heath (formerly newedax, C++ version of Edax)" https://github.com/Nikque/Heath $NEWEDAX_TAG $NEWEDAX_COMMIT \
	"The files of the release ZIP of the tag (64 slots). / タグの Release の ZIP（64 枠）のファイルです。" > "$A/engines/newedax/SOURCE.txt"

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
unzip -l "dist/$NAME.apk" | awk 'NR>3 && $4!="" {printf "%12s  %s\n", $1, $4}' | sort -k2 > "dist/$NAME-files.txt"
sha256sum "dist/$NAME.apk" | tee -a "dist/$NAME-files.txt"
ls -la dist
