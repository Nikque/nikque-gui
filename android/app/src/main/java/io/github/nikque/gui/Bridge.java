// Between the screen (MainActivity, the thread of the screen) and the C++ of the GUI (libnikquegui.so), which runs on
// a thread of its own (run): events go to it with event(); it asks the screen for the windows that wait for an answer
// (a message box, a small menu, a window of settings), and waits for the answer on its thread.
package io.github.nikque.gui;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.Looper;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;

import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.CodingErrorAction;
import java.util.ArrayList;
import java.util.concurrent.CountDownLatch;

final class Bridge {
	private Bridge() {}

	// the kinds of event() (as in src/android/android_bridge.h)
	static final int EV_RESIZE = 1, EV_TOUCH_DOWN = 2, EV_TOUCH_MOVE = 3, EV_TOUCH_UP = 4, EV_RIGHT_CLICK = 5, EV_WHEEL = 6,
			EV_COMMAND = 7, EV_KEY = 8, EV_ACTIVE = 9, EV_CLOSE = 10, EV_SURFACE = 11;

	// the app: runs until its window is closed (called on the thread of the app)
	static native int run(String appDir, String tempDir, float density);
	// an event for the app, from any thread
	static native void event(int kind, int a, int b, int c);
	// the menus of the window now, one line an item: "<depth>\t<command>\t<flags>\t<words>" (flags: 1 checked, 2 grayed,
	// 4 a small menu, 8 a line between items)
	static native String[] menu();

	static volatile MainActivity activity;
	private static final Handler ui = new Handler(Looper.getMainLooper());
	// the window that waits for an answer now (closed with no answer when the screen goes)
	private static AlertDialog open;

	static void screenGone() {
		ui.post(() -> {
			if (open != null) open.dismiss();
		});
	}

	// runs `show` on the thread of the screen and waits until `done` is counted down
	private static void await(Runnable show, CountDownLatch done) {
		ui.post(show);
		for (;;) {
			try {
				done.await();
				return;
			} catch (InterruptedException e) {
				// (waits on)
			}
		}
	}

	private static int dp(Context c, float v) {
		return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, c.getResources().getDisplayMetrics()));
	}

	// ---- asked by the app (on its thread)

	// a message box: kind 0 OK, 1 OK / Cancel, 3 Yes / No / Cancel, 4 Yes / No; the answer as on Windows (1 OK, 2 Cancel,
	// 6 Yes, 7 No)
	static int messageBox(final String text, final String title, final int kind) {
		final int[] answer = {kind == 0 ? 1 : kind == 4 ? 7 : 2};
		final CountDownLatch done = new CountDownLatch(1);
		await(() -> {
			final Activity a = activity;
			if (a == null || a.isFinishing()) {
				done.countDown();
				return;
			}
			final AlertDialog.Builder b = new AlertDialog.Builder(a).setTitle(title).setMessage(text);
			if (kind == 3 || kind == 4) {
				b.setPositiveButton(android.R.string.yes, (d, w) -> answer[0] = 6);
				b.setNegativeButton(android.R.string.no, (d, w) -> answer[0] = 7);
				if (kind == 3) b.setNeutralButton(android.R.string.cancel, (d, w) -> answer[0] = 2);
			} else {
				b.setPositiveButton(android.R.string.ok, (d, w) -> answer[0] = 1);
				if (kind == 1) b.setNegativeButton(android.R.string.cancel, (d, w) -> answer[0] = 2);
			}
			final AlertDialog d = b.create();
			d.setOnDismissListener(x -> {
				if (open == d) open = null;
				done.countDown();
			});
			open = d;
			d.show();
		}, done);
		return answer[0];
	}

	// a small menu (the lines of menu()): the command chosen, 0 when none
	static int popup(final String[] lines) {
		final int[] answer = {0};
		final CountDownLatch done = new CountDownLatch(1);
		await(() -> {
			final Activity a = activity;
			if (a == null || a.isFinishing()) {
				done.countDown();
				return;
			}
			showMenu(a, null, parse(lines), id -> answer[0] = id, done::countDown);
		}, done);
		return answer[0];
	}

	// a window of settings. kinds: 0 number, 1 text, 2 choice, 3 check, 4 heading, 5 number with a slider; choices: the
	// choices of a field, one a line. The answer: for each field its text, the place of its choice, or "1"/"0" (null:
	// cancelled).
	static String[] form(final String title, final int[] kinds, final String[] labels, final String[] texts, final int[] lo,
			final int[] hi, final String[] choices, final int[] choice, final boolean[] checked, final String[] notes,
			final String ok, final String cancel) {
		final String[][] answer = {null};
		final CountDownLatch done = new CountDownLatch(1);
		await(() -> {
			final Activity a = activity;
			if (a == null || a.isFinishing()) {
				done.countDown();
				return;
			}
			final int n = kinds.length, pad = dp(a, 20);
			final LinearLayout col = new LinearLayout(a);
			col.setOrientation(LinearLayout.VERTICAL);
			col.setPadding(pad, dp(a, 8), pad, dp(a, 8));
			final View[] controls = new View[n];
			for (int i = 0; i < n; ++i) {
				if (kinds[i] != 3) {
					final TextView l = new TextView(a);
					l.setText(labels[i]);
					l.setPadding(0, dp(a, 10), 0, 0);
					if (kinds[i] == 4) l.setTypeface(Typeface.DEFAULT_BOLD);
					col.addView(l);
				}
				switch (kinds[i]) {
				case 0:
				case 1:
				case 5: {
					final EditText e = new EditText(a);
					e.setSingleLine(true);
					e.setText(texts[i]);
					if (kinds[i] != 1) e.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_SIGNED);
					col.addView(e);
					controls[i] = e;
					if (kinds[i] == 5) {
						final SeekBar s = new SeekBar(a);
						final int low = lo[i], high = Math.max(hi[i], lo[i] + 1);
						s.setMax(high - low);
						try {
							s.setProgress(Math.max(low, Math.min(high, Integer.parseInt(texts[i].trim()))) - low);
						} catch (NumberFormatException x) {
							// (the slider stays at its start)
						}
						s.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
							@Override public void onProgressChanged(SeekBar bar, int p, boolean user) {
								if (user) e.setText(String.valueOf(p + low));
							}
							@Override public void onStartTrackingTouch(SeekBar bar) {}
							@Override public void onStopTrackingTouch(SeekBar bar) {}
						});
						col.addView(s);
					}
					break;
				}
				case 2: {
					final Spinner s = new Spinner(a);
					final ArrayAdapter<String> ad = new ArrayAdapter<>(a, android.R.layout.simple_spinner_item,
							choices[i].isEmpty() ? new String[0] : choices[i].split("\n", -1));
					ad.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
					s.setAdapter(ad);
					if (choice[i] >= 0 && choice[i] < ad.getCount()) s.setSelection(choice[i]);
					col.addView(s);
					controls[i] = s;
					break;
				}
				case 3: {
					final CheckBox c = new CheckBox(a);
					c.setText(labels[i]);
					c.setChecked(checked[i]);
					col.addView(c);
					controls[i] = c;
					break;
				}
				default: break;
				}
				if (!notes[i].isEmpty()) {
					final TextView t = new TextView(a);
					t.setText(notes[i]);
					t.setAlpha(0.65f);
					t.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
					col.addView(t);
				}
			}
			final ScrollView scroll = new ScrollView(a);
			scroll.addView(col);
			final AlertDialog d = new AlertDialog.Builder(a).setTitle(title).setView(scroll)
					.setPositiveButton(ok, (x, w) -> {
						final String[] r = new String[n];
						for (int i = 0; i < n; ++i) {
							final View c = controls[i];
							if (c instanceof EditText) r[i] = ((EditText) c).getText().toString();
							else if (c instanceof Spinner) r[i] = String.valueOf(((Spinner) c).getSelectedItemPosition());
							else if (c instanceof CheckBox) r[i] = ((CheckBox) c).isChecked() ? "1" : "0";
							else r[i] = "";
						}
						answer[0] = r;
					})
					.setNegativeButton(cancel, null).create();
			d.setOnDismissListener(x -> {
				if (open == d) open = null;
				done.countDown();
			});
			open = d;
			d.show();
		}, done);
		return answer[0];
	}

	static String clipboardGet() {
		final String[] text = {""};
		final CountDownLatch done = new CountDownLatch(1);
		await(() -> {
			try {
				final Activity a = activity;
				final ClipboardManager cm = a == null ? null : (ClipboardManager) a.getSystemService(Context.CLIPBOARD_SERVICE);
				final ClipData cd = cm == null ? null : cm.getPrimaryClip();
				if (cd != null && cd.getItemCount() > 0) {
					final CharSequence s = cd.getItemAt(0).coerceToText(a);
					if (s != null) text[0] = s.toString();
				}
			} catch (RuntimeException e) {
				// (no text)
			}
			done.countDown();
		}, done);
		return text[0];
	}

	static boolean clipboardSet(final String text) {
		final boolean[] ok = {false};
		final CountDownLatch done = new CountDownLatch(1);
		await(() -> {
			try {
				final Activity a = activity;
				final ClipboardManager cm = a == null ? null : (ClipboardManager) a.getSystemService(Context.CLIPBOARD_SERVICE);
				if (cm != null) {
					cm.setPrimaryClip(ClipData.newPlainText("nikque GUI", text));
					ok[0] = true;
				}
			} catch (RuntimeException e) {
				// (not set)
			}
			done.countDown();
		}, done);
		return ok[0];
	}

	// text in a character set (null: it is not text of that set, or the system has no such set)
	static String decode(byte[] bytes, String charset) {
		try {
			final CharBuffer cb = Charset.forName(charset).newDecoder().onMalformedInput(CodingErrorAction.REPORT)
					.onUnmappableCharacter(CodingErrorAction.REPORT).decode(ByteBuffer.wrap(bytes));
			return cb.toString();
		} catch (Exception e) {
			return null;
		}
	}

	// ---- menus as lists in windows (on the thread of the screen)

	static final class Item {
		int id, flags;
		String text;
		final ArrayList<Item> kids = new ArrayList<>();
	}

	interface Chosen {
		void command(int id);
	}

	static ArrayList<Item> parse(String[] lines) {
		final ArrayList<Item> top = new ArrayList<>();
		final ArrayList<ArrayList<Item>> stack = new ArrayList<>();
		stack.add(top);
		for (final String line : lines) {
			final String[] f = line.split("\t", 4);
			if (f.length < 4) continue;
			final int depth = Integer.parseInt(f[0]);
			final Item it = new Item();
			it.id = Integer.parseInt(f[1]);
			it.flags = Integer.parseInt(f[2]);
			it.text = f[3];
			while (stack.size() > depth + 1) stack.remove(stack.size() - 1);
			stack.get(stack.size() - 1).add(it);
			if ((it.flags & 4) != 0) stack.add(it.kids);
		}
		return top;
	}

	// a list of the items; a small menu opens another list over it. `chosen` gets the command; `closed` is called when
	// the first list has closed (with or without a choice).
	static void showMenu(final Activity a, final String title, final ArrayList<Item> items, final Chosen chosen, final Runnable closed) {
		final ArrayList<Item> shown = new ArrayList<>();
		for (final Item it : items)
			if ((it.flags & 8) == 0) shown.add(it);
		final String[] words = new String[shown.size()];
		for (int i = 0; i < words.length; ++i) {
			final Item it = shown.get(i);
			words[i] = ((it.flags & 1) != 0 ? "✓ " : "") + it.text + ((it.flags & 4) != 0 ? "  ▸" : "");
		}
		final ArrayAdapter<String> ad = new ArrayAdapter<String>(a, android.R.layout.simple_list_item_1, words) {
			@Override public boolean areAllItemsEnabled() { return false; }
			@Override public boolean isEnabled(int i) { return (shown.get(i).flags & 2) == 0; }
			@Override public View getView(int i, View v, ViewGroup parent) {
				final View row = super.getView(i, v, parent);
				row.setAlpha(isEnabled(i) ? 1.0f : 0.4f);
				return row;
			}
		};
		final ListView list = new ListView(a);
		list.setAdapter(ad);
		final AlertDialog.Builder b = new AlertDialog.Builder(a).setView(list);
		if (title != null) b.setTitle(title);
		final AlertDialog d = b.create();
		final boolean[] picked = {false};
		list.setOnItemClickListener((AdapterView<?> p, View v, int i, long rowId) -> {
			final Item it = shown.get(i);
			if ((it.flags & 4) != 0) {
				// (a choice in the small menu closes this list too)
				showMenu(a, it.text, it.kids, id -> {
					picked[0] = true;
					chosen.command(id);
					d.dismiss();
				}, null);
				return;
			}
			picked[0] = true;
			chosen.command(it.id);
			d.dismiss();
		});
		d.setOnDismissListener(x -> {
			if (open == d) open = null;
			if (closed != null) closed.run();
		});
		if (closed != null) open = d;
		d.getWindow().setGravity(Gravity.CENTER);
		d.show();
	}
}
