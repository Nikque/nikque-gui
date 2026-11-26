#!/bin/bash
# Run the exact PGO engine files packaged for the emulator in Android's execution environment.
set -euo pipefail

apks=(dist/*-android-ci.apk)
[ "${#apks[@]}" -eq 1 ] && [ -f "${apks[0]}" ]
apk=${apks[0]}
host=$(mktemp -d)
device=/data/local/tmp/nikque-pgo-smoke
trap 'rm -rf -- "$host"; adb shell rm -rf "$device" >/dev/null' EXIT
mkdir -p run
adb shell mkdir -p "$device"
adb push nk/problem/fforum-1-19.obf "$device/positions.obf" > /dev/null
for engine in nikque newedax; do
    unzip -p "$apk" "lib/x86_64/libedax_$engine.so" > "$host/aEdax"
    unzip -p "$apk" "assets/app/engines/$engine/data/eval.dat" > "$host/eval.dat"
    adb push "$host/aEdax" "$device/aEdax" > /dev/null
    adb push "$host/eval.dat" "$device/eval.dat" > /dev/null
    adb shell chmod 700 "$device/aEdax"
    log="run/pgo-$engine-x86_64.log"
    adb shell "cd '$device' && ./aEdax -n 1 -l 60 -eval-file eval.dat -book-file smoke-book.dat -solve positions.obf" |
        tee "$log"
    grep -q '19 positions; 0 erroneous move; 0 erroneous score;' "$log"
done
