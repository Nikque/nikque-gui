#!/bin/bash
# Build and verify an Android engine with a profile collected from its actual target ABI.
set -euo pipefail

abi=$1
src=$(realpath "$2")
weights=$(realpath "$3")
destination=$4
report=$(realpath -m "$5")
ndk=$(realpath "$ANDROID_NDK")
toolchain=$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin
runs=()
case "$abi" in
    arm64-v8a) runs=(qemu-aarch64); machine=AArch64 ;;
    x86_64) machine='Advanced Micro Devices X86-64' ;;
    *) echo "Unsupported Android ABI: $abi" >&2; exit 1 ;;
esac
mkdir -p "$report"
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
profile=$work/engine.profdata

ndk_build() {
    local stage=$1
    local flags=$2
    local link_flags=$3
    if ! (cd "$src/src" && "$ndk/ndk-build" NDK_PROJECT_PATH=. NDK_APPLICATION_MK=./Application.mk \
        APP_ABI="$abi" APP_PLATFORM=android-29 APP_CFLAGS="$flags" APP_LDFLAGS="$link_flags" \
        NDK_OUT="$work/$stage/obj" NDK_LIBS_OUT="$work/$stage/lib" -j2) > "$report/$stage-build.log" 2>&1; then
        tail -80 "$report/$stage-build.log" >&2
        exit 1
    fi
}

train() {
    local stage=$1
    shift
    (cd "$work" && LLVM_PROFILE_FILE="$work/$stage.profraw" timeout 300 "${runs[@]}" "$instrumented" \
        -n 1 -eval-file "$weights" -book-file training-book.dat "$@" < /dev/null) > "$report/$stage.log" 2>&1 ||
        { tail -80 "$report/$stage.log" >&2; exit 1; }
    [ -s "$work/$stage.profraw" ] || { echo "No Android $abi profile from $stage" >&2; exit 1; }
}

# Statically link the training executable so its target Bionic code runs under QEMU user mode
# without an Android filesystem. Keep the APK executable dynamically linked and position-independent.
ndk_build instrument -fprofile-instr-generate '-static -fprofile-instr-generate'
instrumented=$work/instrument/lib/$abi/aEdax
train solve -l 60 -solve "$src/problem/fforum-20-39.obf"
grep -q '20 positions; 0 erroneous move; 0 erroneous score;' "$report/solve.log"
train play -l 8 -auto-store on -auto-start on -repeat 2 -auto-quit on -mode 2 -book-seed 1
grep -q '@save ok=1 file=training-book.dat' "$report/play.log"
[ "$(grep -c 'Game Over' "$report/play.log")" = 2 ]

"$toolchain/llvm-profdata" merge "$work/solve.profraw" "$work/play.profraw" -o "$profile"
"$toolchain/llvm-profdata" show --counts "$profile" > "$report/profile.txt"
grep -Eq '^Total functions: [1-9][0-9]*$' "$report/profile.txt"
grep -Eq '^Maximum function count: [1-9][0-9]*$' "$report/profile.txt"
pgo_flags="-fprofile-instr-use=$profile -Werror=profile-instr-out-of-date"
ndk_build final "$pgo_flags" ""
final=$work/final/lib/$abi/aEdax
"$toolchain/llvm-readelf" -h -l -S -d "$final" > "$report/elf.txt"
grep -Eq "Machine:[[:space:]]+$machine" "$report/elf.txt"
grep -Eq 'Type:[[:space:]]+DYN' "$report/elf.txt"
grep -Eq 'INTERP' "$report/elf.txt"
if grep -Eq '__llvm_prf' "$report/elf.txt"; then
    echo "Unexpected profile runtime in final Android $abi executable" >&2
    exit 1
fi
# Relink profiled code with static Bionic for an ABI-level solve check on the host.
ndk_build check "$pgo_flags" "-static"
verified=$work/check/lib/$abi/aEdax
python3 "$src/tests/check-solve.py" "$verified" "$src/problem/fforum-1-19.obf" \
    --prefix "${runs[*]}" --cwd "$work" --eval "$weights" > "$report/final-solve.log" 2>&1 ||
    { tail -80 "$report/final-solve.log" >&2; exit 1; }
cp "$final" "$destination"
{
    echo "ABI: $abi"
    "$toolchain/clang" --version | sed -n '1p'
    sed -n '/^Total functions:/p; /^Maximum function count:/p' "$report/profile.txt"
    tail -1 "$report/final-solve.log"
    sha256sum "$profile" "$destination" | awk '{print $1}'
} > "$report/result.txt"
cat "$report/result.txt"
