#!/bin/bash
# Run the exact packaged PGO executables, then start the signed arm64 APK.
set -euo pipefail

arm_apks=(dist/*-android.apk)
x86_apks=(dist/*-android-ci.apk)
[ "${#arm_apks[@]}" -eq 1 ] && [ -f "${arm_apks[0]}" ]
[ "${#x86_apks[@]}" -eq 1 ] && [ -f "${x86_apks[0]}" ]
host=$(mktemp -d)
device=$(adb shell mktemp -d /data/local/tmp/nikque-pgo-smoke.XXXXXX | tr -d '\r')
case "$device" in /data/local/tmp/nikque-pgo-smoke.*) ;; *) exit 1 ;; esac
pkg=io.github.nikque.gui
installed=false
cleanup() {
    if [ "$installed" = true ]; then adb uninstall "$pkg" > /dev/null || true; fi
    adb shell rm -rf "$device" > /dev/null || true
    rm -rf -- "$host"
}
trap cleanup EXIT
mkdir -p run
adb shell getprop ro.product.cpu.abilist64 > run/pgo-emulator-abis.txt
grep -qw arm64-v8a run/pgo-emulator-abis.txt
adb push nk/problem/fforum-1-19.obf "$device/positions.obf" > /dev/null

check_engines() {
    local apk=$1 abi=$2 engine log
    for engine in nikque newedax; do
        unzip -p "$apk" "lib/$abi/libedax_$engine.so" > "$host/aEdax"
        unzip -p "$apk" "assets/app/engines/$engine/data/eval.dat" > "$host/eval.dat"
        adb push "$host/aEdax" "$device/aEdax" > /dev/null
        adb push "$host/eval.dat" "$device/eval.dat" > /dev/null
        adb shell chmod 700 "$device/aEdax"
        log="run/pgo-$engine-$abi.log"
        adb shell "cd '$device' && ./aEdax -n 1 -l 60 -eval-file eval.dat -book-file smoke-book.dat -solve positions.obf" |
            tee "$log"
        grep -q '19 positions; 0 erroneous move; 0 erroneous score;' "$log"
    done
}
check_engines "${x86_apks[0]}" x86_64
check_engines "${arm_apks[0]}" arm64-v8a

adb install --abi arm64-v8a "${arm_apks[0]}"
installed=true
adb shell dumpsys package "$pkg" > run/pgo-arm64-package.txt
grep -q 'primaryCpuAbi=arm64-v8a' run/pgo-arm64-package.txt
adb logcat -c
adb shell am start -W -n "$pkg/.MainActivity" | tee run/pgo-arm64-start.txt
grep -q '^Status: ok' run/pgo-arm64-start.txt
sleep 20
adb logcat -d -v brief > run/pgo-arm64-logcat.txt
adb shell pidof "$pkg" | tee run/pgo-arm64-pid.txt
test -s run/pgo-arm64-pid.txt
adb exec-out screencap -p > run/pgo-arm64-main.png
adb shell am force-stop "$pkg"
