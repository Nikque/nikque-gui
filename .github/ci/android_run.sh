#!/bin/bash
# The APK of the CI on the emulator (.github/workflows/android.yml): installed, started (it runs smoke.txt at its start
# and ends by itself), pictures of the whole screen meanwhile, then its pictures, its log and its config.ini taken out.
set -uo pipefail

APK=$(ls dist/nikque-gui-*-android-ci.apk | head -1)
PKG=io.github.nikque.gui
mkdir -p run
# Remove only the artifacts expected from this smoke run.
while IFS= read -r line || [ -n "$line" ]; do
	case $line in
	"screenshot "?* | "savelog "?*) rm -f -- "run/${line#* }" ;;
	esac
done < .github/ci/smoke-android.txt
adb install -r "$APK" || exit 1
adb shell pm clear "$PKG" | grep -q "^Success" || exit 1
adb logcat -c
adb shell am start -n $PKG/.MainActivity || exit 1

# (the script takes about five minutes; the app ends at its end)
for i in $(seq 1 90); do
	sleep 10
	if [ "$i" = 6 ] || [ "$i" = 14 ] || [ "$i" = 24 ]; then adb exec-out screencap -p > "run/screen-$i.png"; fi
	if [ "$i" = 16 ]; then
		# (the list of the menus, opened by a tap on the first button of the row)
		adb shell input tap 150 180
		sleep 3
		adb exec-out screencap -p > run/screen-menu.png
		adb shell input keyevent KEYCODE_BACK
	fi
	if ! adb shell pidof $PKG > /dev/null; then echo "the app has ended after $((i * 10)) s"; break; fi
done
# failed: the app still running at the time limit (the script did not end), or a file of the script missing
fail=0
if adb shell pidof $PKG > /dev/null; then
	echo "timeout: the app is still running"
	fail=1
fi

adb logcat -d -v brief > run/logcat-all.txt
if ! grep -qE "I/NikqueSmoke.*native exit code: 0[[:space:]]*$" run/logcat-all.txt; then
	echo "native GUI did not report a successful exit"
	fail=1
fi
grep -E "nikque|AndroidRuntime|DEBUG|libc|SELinux|avc:" run/logcat-all.txt | tail -300 > run/logcat.txt
adb exec-out run-as $PKG sh -c 'cd files/app && tar cf - *.png *.txt config.ini 2>/dev/null' > run/files.tar
tar -tf run/files.tar > /dev/null 2>&1 && tar -C run -xf run/files.tar
# (the files are taken one by one when the tar cannot be read)
if ! ls run/android-*.png > /dev/null 2>&1; then
	for f in $(adb shell run-as $PKG ls files/app | tr -d '\r'); do
		case $f in *.png|*.txt|config.ini) adb exec-out run-as $PKG cat "files/app/$f" > "run/$f" ;; esac
	done
fi
adb shell run-as $PKG ls -la files/app files/app/engines/nikque files/app/engines/newedax > run/files-list.txt 2>&1
bash .github/ci/check_smoke.sh .github/ci/smoke-android.txt run || fail=1
adb shell am force-stop $PKG

# the app once more without the script: pictures of the whole screen (the row of buttons, the list of the menus, a
# move played by a tap, the question of the Back button), then it is stopped
adb shell "run-as $PKG sed -i 's/^startup=.*/startup=/' files/app/config.ini"
adb shell am start -n $PKG/.MainActivity
sleep 25
adb exec-out screencap -p > run/screen-1-start.png
adb shell input tap 150 190
sleep 3
adb exec-out screencap -p > run/screen-2-menu.png
adb shell input tap 540 1000
sleep 3
adb exec-out screencap -p > run/screen-3-submenu.png
adb shell input keyevent KEYCODE_BACK
sleep 1
adb shell input keyevent KEYCODE_BACK
sleep 2
adb shell input tap 500 700
sleep 12
adb exec-out screencap -p > run/screen-4-tap.png
adb shell input swipe 500 900 500 900 900
sleep 4
adb exec-out screencap -p > run/screen-5-hold.png
adb shell input keyevent KEYCODE_BACK
sleep 2
adb exec-out screencap -p > run/screen-6-back.png
adb logcat -d -v brief | grep -E "nikque|AndroidRuntime|F/DEBUG|F/libc" | tail -80 > run/logcat-2.txt
adb shell am force-stop $PKG
rm -f run/files.tar run/logcat-all.txt
ls -la run
exit $fail
