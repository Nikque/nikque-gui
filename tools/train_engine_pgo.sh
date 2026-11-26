#!/bin/bash
set -euo pipefail

# Each invocation runs in one fresh export of a pinned engine source tree.
src=$1 exe=$2 eval_file=$3 problems=$4 evidence=$5
shift 5
mkdir -p "$src/bin/data" "$evidence"
cp "$eval_file" "$src/bin/data/eval.dat"
cd "$src/src"
case $(uname -s) in
    Linux)
        generate=(-fprofile-generate -fprofile-update=atomic)
        use=(-fprofile-use -fprofile-correction -Werror=missing-profile -Werror=coverage-mismatch)
        ;;
    Darwin)
        generate=(-fprofile-instr-generate -fprofile-update=atomic)
        use=(-fprofile-instr-use=edax.profdata -Werror=profile-instr-out-of-date -Werror=profile-instr-unprofiled)
        export LLVM_PROFILE_FILE="$PWD/train-%p.profraw"
        ;;
    *) echo "Unsupported PGO host" >&2; exit 1 ;;
esac

run_logged() {
    local log=$1
    shift
    printf '%q ' "$@" > "$log"
    printf '\n' >> "$log"
    "$@" >> "$log" 2>&1 || { tail -80 "$log"; return 1; }
}
sha256() { shasum -a 256 "$1" | cut -d' ' -f1; }

run_logged "$evidence/instrument.txt" make pgo-rebuild "$@" "PGO_USE=${generate[*]}"
instrumented=$(sha256 "../bin/$exe")
(
    cd ../bin
    common=(-n 1 -book-usage off -book-file data/pgo-synthetic.dat)
    run_logged "$evidence/train-solve.txt" "./$exe" "${common[@]}" -l 60 -solve "$problems"
    grep -Fq '19 positions; 0 erroneous move; 0 erroneous score;' "$evidence/train-solve.txt"
    run_logged "$evidence/train-play.txt" "./$exe" "${common[@]}" -l 12 -auto-start on -auto-quit on -repeat 2 -mode 2 < /dev/null
    [ "$(grep -Fc '*** Game Over ***' "$evidence/train-play.txt")" -eq 2 ]
)
case $(uname -s) in
    Linux)
        profiles=(../bin/*.gcda)
        ;;
    Darwin)
        raw=(train-*.profraw)
        [ "${#raw[@]}" -ge 2 ] && [ -s "${raw[0]}" ]
        run_logged "$evidence/merge.txt" xcrun llvm-profdata merge -o edax.profdata "${raw[@]}"
        run_logged "$evidence/profile.txt" xcrun llvm-profdata show --counts edax.profdata
        profiles=(edax.profdata)
        ;;
esac
[ -s "${profiles[0]}" ]
for file in "${profiles[@]}"; do
    printf '%s  %s\n' "$(sha256 "$file")" "${file##*/}"
done > "$evidence/profile-sha256.txt"

run_logged "$evidence/final-link.txt" make pgo-rebuild "$@" "PGO_USE=${use[*]}"
[ "$(sha256 "../bin/$exe")" != "$instrumented" ]
(
    cd ../bin
    run_logged "$evidence/final-solve.txt" "./$exe" -n 1 -book-usage off -book-file data/pgo-synthetic.dat -l 60 -solve "$problems"
    grep -Fq '19 positions; 0 erroneous move; 0 erroneous score;' "$evidence/final-solve.txt"
    shasum -a 256 "$exe" > "$evidence/binary.sha256"
)
