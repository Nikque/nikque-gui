// The screen of nikque GUI on Android: a row of buttons (the menus, the moves back and forth) over the view the C++
// of the GUI draws (the board, the panel, the log). The files of the app (config.ini, the words, the eval files of the
// engines) are taken out of the APK at the first start of a version; the engines are programs in the folder of the
// native libraries, linked into the folders the GUI looks for them in.
package io.github.nikque.gui;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.system.Os;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
	static {
		System.loadLibrary("nikquegui");
	}

	// the commands of the buttons (cmd::Undo ... of src/app.h; checked in src/android/app_android.cpp)
	private static final int CMD_UNDO = 1001, CMD_REDO = 1002, CMD_TO_START = 1003, CMD_TO_END = 1004;
	// the engines: the folder of each in the files of the app, and its program among the native libraries
	private static final String[][] ENGINES = {{"nikque", "libedax_nikque.so"}, {"newedax", "libedax_newedax.so"}};

	private static boolean started;  // the thread of the app runs (one for the process)
	private final Handler handler = new Handler(Looper.getMainLooper());
	private TextView waiting;

	@Override
	protected void onCreate(Bundle saved) {
		super.onCreate(saved);
		Bridge.activity = this;

		final int bg = Color.rgb(0x18, 0x18, 0x1b);
		final LinearLayout root = new LinearLayout(this);
		root.setOrientation(LinearLayout.VERTICAL);
		root.setBackgroundColor(bg);
		// (the screen goes under the bars of the system: the views keep off them)
		root.setOnApplyWindowInsetsListener((v, insets) -> {
			if (android.os.Build.VERSION.SDK_INT >= 30) {
				final android.graphics.Insets in = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
				v.setPadding(in.left, in.top, in.right, in.bottom);
			} else {
				v.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(), insets.getSystemWindowInsetRight(),
						insets.getSystemWindowInsetBottom());
			}
			return insets;
		});

		final LinearLayout bar = new LinearLayout(this);
		bar.setOrientation(LinearLayout.HORIZONTAL);
		bar.setGravity(Gravity.CENTER_VERTICAL);
		bar.addView(button("≡  " + getString(R.string.menu), 2.2f, v -> openMenu()));
		bar.addView(button("|◀", 1, v -> Bridge.event(Bridge.EV_COMMAND, CMD_TO_START, 0, 0)));
		bar.addView(button("◀", 1.4f, v -> Bridge.event(Bridge.EV_COMMAND, CMD_UNDO, 0, 0)));
		bar.addView(button("▶", 1.4f, v -> Bridge.event(Bridge.EV_COMMAND, CMD_REDO, 0, 0)));
		bar.addView(button("▶|", 1, v -> Bridge.event(Bridge.EV_COMMAND, CMD_TO_END, 0, 0)));
		root.addView(bar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(46)));

		final FrameLayout frame = new FrameLayout(this);
		final SurfaceView view = new SurfaceView(this);
		view.getHolder().addCallback(this);
		view.setOnTouchListener((v, e) -> touch(e));
		view.setFocusable(true);
		view.setFocusableInTouchMode(true);
		frame.addView(view, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
		waiting = new TextView(this);
		waiting.setText(R.string.preparing);
		waiting.setTextColor(Color.rgb(0xe4, 0xe4, 0xe7));
		waiting.setBackgroundColor(bg);
		waiting.setGravity(Gravity.CENTER);
		frame.addView(waiting, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
		root.addView(frame, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
		setContentView(root);
		view.requestFocus();

		if (started) {
			waiting.setVisibility(View.GONE);
			return;
		}
		start();
	}

	// The thread of the app: the files, then the C++ of the GUI until its window is closed. When the files cannot be
	// prepared, the message says so, and a tap on it (or the screen made again) tries again.
	private void start() {
		started = true;
		final float density = getResources().getDisplayMetrics().density;
		final File app = new File(getFilesDir(), "app");
		final File temp = getCacheDir();
		new Thread(null, () -> {
			String error = null;
			try {
				prepareFiles(app);
			} catch (Exception e) {
				error = e.toString();
			}
			final String failed = error;
			handler.post(() -> {
				if (failed != null) started = false;
				final MainActivity a = Bridge.activity;
				if (a == null) return;
				if (failed == null) {
					a.waiting.setVisibility(View.GONE);
					return;
				}
				a.waiting.setText(a.getString(R.string.prepare_failed) + "\n" + failed + "\n\n" + a.getString(R.string.prepare_retry));
				a.waiting.setOnClickListener(v -> {
					if (started) return;
					v.setOnClickListener(null);
					a.waiting.setText(R.string.preparing);
					a.start();
				});
			});
			if (failed != null) return;
			Bridge.run(new File(app, "nikque-gui").getAbsolutePath(), temp.getAbsolutePath() + "/", density);
			handler.post(() -> {
				final MainActivity a = Bridge.activity;
				if (a != null) a.finishAndRemoveTask();
				// (the app has ended: the process goes, and the engines with it)
				handler.postDelayed(() -> System.exit(0), 300);
			});
		}, "nikque-app", 16L << 20).start();
	}

	@Override
	protected void onResume() {
		super.onResume();
		Bridge.activity = this;
		Bridge.event(Bridge.EV_ACTIVE, 1, 0, 0);
	}

	@Override
	protected void onPause() {
		super.onPause();
		Bridge.event(Bridge.EV_ACTIVE, 0, 0, 0);
	}

	@Override
	protected void onDestroy() {
		if (Bridge.activity == this) {
			Bridge.screenGone();
			Bridge.activity = null;
		}
		super.onDestroy();
	}

	// the back button: the app ends when the user says so
	@Override
	public void onBackPressed() {
		new AlertDialog.Builder(this).setMessage(R.string.exit_ask)
				.setPositiveButton(R.string.exit_yes, (d, w) -> Bridge.event(Bridge.EV_CLOSE, 0, 0, 0))
				.setNegativeButton(android.R.string.cancel, null).show();
	}

	private int dp(float v) {
		return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, getResources().getDisplayMetrics()));
	}

	private Button button(String text, float weight, View.OnClickListener click) {
		final Button b = new Button(this, null, android.R.attr.borderlessButtonStyle);
		b.setText(text);
		b.setAllCaps(false);
		b.setTextColor(Color.rgb(0xf4, 0xf4, 0xf5));
		b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
		b.setPadding(0, 0, 0, 0);
		b.setOnClickListener(click);
		b.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, weight));
		return b;
	}

	private void openMenu() {
		final String[] lines = Bridge.menu();
		if (lines == null || lines.length == 0) return;
		Bridge.showMenu(this, null, Bridge.parse(lines), id -> Bridge.event(Bridge.EV_COMMAND, id, 0, 0), null);
	}

	// ---- the view

	@Override
	public void surfaceCreated(SurfaceHolder h) {
		synchronized (Draw.surfaceLock) {
			Draw.holder = h;
		}
		Bridge.event(Bridge.EV_SURFACE, 1, 0, 0);
	}

	@Override
	public void surfaceChanged(SurfaceHolder h, int format, int w, int height) {
		synchronized (Draw.surfaceLock) {
			Draw.holder = h;
		}
		Bridge.event(Bridge.EV_RESIZE, w, height, 0);
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder h) {
		synchronized (Draw.surfaceLock) {
			if (Draw.holder == h) Draw.holder = null;
		}
		Bridge.event(Bridge.EV_SURFACE, 0, 0, 0);
	}

	// A finger as the mouse: a tap is a click (two taps soon after each other: a double click), a finger held still is a
	// right click, a finger moved is a drag; two fingers moved up or down turn the wheel.
	private float downX, downY, lastTapX, lastTapY, scrollY, scrollRest;
	private long lastTap;
	private boolean dragging, held, scrolling;
	private final Runnable hold = () -> {
		if (dragging || scrolling) return;
		held = true;
		Bridge.event(Bridge.EV_RIGHT_CLICK, Math.round(downX), Math.round(downY), 0);
	};

	private boolean touch(MotionEvent e) {
		final int slop = ViewConfiguration.get(this).getScaledTouchSlop();
		final int x = Math.round(e.getX()), y = Math.round(e.getY());
		switch (e.getActionMasked()) {
		case MotionEvent.ACTION_DOWN:
			downX = e.getX();
			downY = e.getY();
			dragging = held = scrolling = false;
			handler.postDelayed(hold, ViewConfiguration.getLongPressTimeout());
			return true;
		case MotionEvent.ACTION_POINTER_DOWN:
			handler.removeCallbacks(hold);
			if (dragging) Bridge.event(Bridge.EV_TOUCH_UP, x, y, 0);
			dragging = false;
			scrolling = true;
			scrollY = (e.getY(0) + e.getY(e.getPointerCount() - 1)) / 2;
			scrollRest = 0;
			return true;
		case MotionEvent.ACTION_MOVE:
			if (held) return true;
			if (scrolling) {
				if (e.getPointerCount() < 2) return true;
				final float now = (e.getY(0) + e.getY(1)) / 2, step = dp(40);
				scrollRest += now - scrollY;
				scrollY = now;
				while (Math.abs(scrollRest) >= step) {
					final int dir = scrollRest > 0 ? 1 : -1;
					scrollRest -= dir * step;
					Bridge.event(Bridge.EV_WHEEL, Math.round(downX), Math.round(downY), dir * 120);
				}
				return true;
			}
			if (!dragging) {
				if (Math.hypot(e.getX() - downX, e.getY() - downY) <= slop) return true;
				handler.removeCallbacks(hold);
				dragging = true;
				Bridge.event(Bridge.EV_TOUCH_DOWN, Math.round(downX), Math.round(downY), 0);
			}
			Bridge.event(Bridge.EV_TOUCH_MOVE, x, y, 0);
			return true;
		case MotionEvent.ACTION_UP:
		case MotionEvent.ACTION_CANCEL:
			handler.removeCallbacks(hold);
			if (held || scrolling) return true;
			if (dragging) {
				Bridge.event(Bridge.EV_TOUCH_UP, x, y, 0);
				return true;
			}
			if (e.getActionMasked() == MotionEvent.ACTION_CANCEL) return true;
			final long now = e.getEventTime();
			final boolean twice = lastTap != 0 && now - lastTap <= ViewConfiguration.getDoubleTapTimeout() &&
					Math.hypot(e.getX() - lastTapX, e.getY() - lastTapY) <= 3 * slop;
			lastTap = twice ? 0 : now;
			lastTapX = e.getX();
			lastTapY = e.getY();
			Bridge.event(Bridge.EV_TOUCH_DOWN, x, y, twice ? 1 : 0);
			Bridge.event(Bridge.EV_TOUCH_UP, x, y, 0);
			return true;
		default:
			return true;
		}
	}

	// the keys of a keyboard, as the virtual keys of Windows
	@Override
	public boolean onKeyDown(int code, KeyEvent e) {
		int vk = 0;
		switch (code) {
		case KeyEvent.KEYCODE_DPAD_LEFT: vk = 0x25; break;
		case KeyEvent.KEYCODE_DPAD_UP: vk = 0x26; break;
		case KeyEvent.KEYCODE_DPAD_RIGHT: vk = 0x27; break;
		case KeyEvent.KEYCODE_DPAD_DOWN: vk = 0x28; break;
		case KeyEvent.KEYCODE_MOVE_HOME: vk = 0x24; break;
		case KeyEvent.KEYCODE_MOVE_END: vk = 0x23; break;
		case KeyEvent.KEYCODE_PAGE_UP: vk = 0x21; break;
		case KeyEvent.KEYCODE_PAGE_DOWN: vk = 0x22; break;
		case KeyEvent.KEYCODE_ENTER:
		case KeyEvent.KEYCODE_NUMPAD_ENTER: vk = 0x0d; break;
		case KeyEvent.KEYCODE_ESCAPE: vk = 0x1b; break;
		case KeyEvent.KEYCODE_DEL: vk = 0x08; break;
		case KeyEvent.KEYCODE_FORWARD_DEL: vk = 0x2e; break;
		case KeyEvent.KEYCODE_SPACE: vk = 0x20; break;
		case KeyEvent.KEYCODE_TAB: vk = 0x09; break;
		default:
			if (code >= KeyEvent.KEYCODE_A && code <= KeyEvent.KEYCODE_Z) vk = 'A' + (code - KeyEvent.KEYCODE_A);
			else if (code >= KeyEvent.KEYCODE_0 && code <= KeyEvent.KEYCODE_9) vk = '0' + (code - KeyEvent.KEYCODE_0);
			else if (code >= KeyEvent.KEYCODE_F1 && code <= KeyEvent.KEYCODE_F12) vk = 0x70 + (code - KeyEvent.KEYCODE_F1);
			break;
		}
		if (vk == 0 || e.isAltPressed()) return super.onKeyDown(code, e);
		Bridge.event(Bridge.EV_KEY, vk, (e.isCtrlPressed() ? 1 : 0) | (e.isShiftPressed() ? 2 : 0), 0);
		return true;
	}

	// ---- the files of the app

	// The folder "app" of the assets into the files of the app, when this version of the APK has not done it yet:
	// config.ini only when there is none (it is the user's); the engines linked from the folder of the native libraries
	// (each time: that folder moves when the app is updated).
	private void prepareFiles(File app) throws Exception {
		final PackageInfo pi = getPackageManager().getPackageInfo(getPackageName(), 0);
		final String stamp = pi.versionName + " " + pi.lastUpdateTime;
		final File mark = new File(app, ".assets");
		String have = "";
		if (mark.isFile()) {
			try (InputStream in = new java.io.FileInputStream(mark)) {
				have = new String(readAll(in), StandardCharsets.UTF_8);
			}
		}
		if (!stamp.equals(have)) {
			copyAssets(getAssets(), "app", app);
			try (OutputStream out = new FileOutputStream(mark)) {
				out.write(stamp.getBytes(StandardCharsets.UTF_8));
			}
		}
		final String libs = getApplicationInfo().nativeLibraryDir;
		for (final String[] e : ENGINES) {
			final File dir = new File(app, "engines/" + e[0]);
			final File program = new File(libs, e[1]);
			if (!dir.isDirectory() || !program.isFile()) continue;
			final File link = new File(dir, "aEdax");
			try {
				Os.remove(link.getAbsolutePath());
			} catch (Exception x) {
				// (there was none)
			}
			Os.symlink(program.getAbsolutePath(), link.getAbsolutePath());
		}
	}

	private static byte[] readAll(InputStream in) throws Exception {
		final java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
		final byte[] buf = new byte[1 << 16];
		for (int n; (n = in.read(buf)) > 0;) out.write(buf, 0, n);
		return out.toByteArray();
	}

	private static void copyAssets(AssetManager am, String from, File to) throws Exception {
		final String[] names = am.list(from);
		if (names == null || names.length == 0) {
			// a file
			if (to.getName().equals("config.ini") && to.exists()) return;
			final File tmp = new File(to.getPath() + ".new");
			try (InputStream in = am.open(from); OutputStream out = new FileOutputStream(tmp)) {
				final byte[] buf = new byte[1 << 16];
				for (int n; (n = in.read(buf)) > 0;) out.write(buf, 0, n);
			}
			if (!tmp.renameTo(to)) throw new java.io.IOException("cannot write " + to);
			return;
		}
		if (!to.isDirectory() && !to.mkdirs()) throw new java.io.IOException("cannot make " + to);
		for (final String n : names) copyAssets(am, from + "/" + n, new File(to, n));
	}
}
