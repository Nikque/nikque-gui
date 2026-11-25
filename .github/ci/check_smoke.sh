#!/bin/bash
# The files a smoke script writes: each "screenshot <file>" and "savelog <file>" of the script must be in the folder
# and not be empty. A script stopped by an error writes none of the files after that line (the last savelog is the
# last line before quit).
# usage: check_smoke.sh <script> <folder>
set -u
script=$1
dir=$2
bad=0
n=0
while IFS= read -r line || [ -n "$line" ]; do
	line=${line%$'\r'}
	case $line in
	"screenshot "?* | "savelog "?*)
		f=${line#* }
		n=$((n + 1))
		if [ ! -s "$dir/$f" ]; then
			echo "missing: $f"
			bad=1
		fi
		;;
	esac
done < "$script"
if [ "$n" = 0 ]; then
	echo "no files are named in $script"
	bad=1
fi
[ "$bad" = 0 ] && echo "all $n files of $script are there"
exit $bad
