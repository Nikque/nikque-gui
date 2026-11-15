// The drawing of the window on Android: the C++ of the GUI (src/android/gfx_android.cpp) draws a frame with these
// calls into a picture of the size of the view, then puts it on the screen (present). Every call comes from the thread
// of the app (Bridge.run); only the surface is shared with the thread of the screen.
package io.github.nikque.gui;

import android.graphics.Bitmap;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.util.SparseArray;
import android.view.SurfaceHolder;

import java.io.FileOutputStream;
import java.util.Locale;

final class Draw {
	private Draw() {}

	private static Bitmap frame;          // the picture of the window
	private static Canvas frameCanvas;
	private static Bitmap image;          // a picture being made for a file (beginImage .. endImage)
	private static Canvas canvas;         // where the calls draw now
	private static final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
	private static final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
	private static final Paint bitmapPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
	private static final Matrix matrix = new Matrix();
	private static boolean identity = true;
	private static final float[] values = new float[9];
	private static final RectF rect = new RectF();
	private static final Path scratch = new Path();
	private static final SparseArray<Path> paths = new SparseArray<>();
	private static final SparseArray<Bitmap> glows = new SparseArray<>();
	private static int nextId = 1;
	private static final Typeface normal = Typeface.create(Typeface.SANS_SERIF, Typeface.NORMAL);
	private static final Typeface bold = Typeface.create(Typeface.SANS_SERIF, Typeface.BOLD);

	// the surface of the view (set by the thread of the screen)
	static final Object surfaceLock = new Object();
	static SurfaceHolder holder;

	// ---- the frame

	static void resize(int w, int h) {
		if (frame != null && frame.getWidth() == w && frame.getHeight() == h) return;
		frame = Bitmap.createBitmap(Math.max(1, w), Math.max(1, h), Bitmap.Config.ARGB_8888);
		frameCanvas = new Canvas(frame);
		canvas = frameCanvas;
	}

	static void beginFrame() {
		canvas = frameCanvas;
	}

	// the frame (or its part, when r > l) goes to the screen
	static void present(int l, int t, int r, int b) {
		synchronized (surfaceLock) {
			if (holder == null || frame == null) return;
			Canvas c = null;
			try {
				c = r > l ? holder.lockCanvas(new Rect(l, t, r, b)) : holder.lockCanvas();
				if (c != null) c.drawBitmap(frame, 0, 0, null);
			} catch (RuntimeException e) {
				// (the surface went meanwhile)
			} finally {
				if (c != null) {
					try {
						holder.unlockCanvasAndPost(c);
					} catch (RuntimeException e) {
						// (as above)
					}
				}
			}
		}
	}

	// a frame drawn into a picture of its own, for a file
	static void beginImage() {
		image = Bitmap.createBitmap(frame.getWidth(), frame.getHeight(), Bitmap.Config.ARGB_8888);
		canvas = new Canvas(image);
	}

	static boolean endImage(String path, int l, int t, int r, int b) {
		canvas = frameCanvas;
		final Bitmap all = image;
		image = null;
		if (all == null) return false;
		try (FileOutputStream out = new FileOutputStream(path)) {
			final Bitmap part = Bitmap.createBitmap(all, l, t, r - l, b - t);
			return part.compress(Bitmap.CompressFormat.PNG, 100, out);
		} catch (Exception e) {
			return false;
		}
	}

	// ---- the brush and the transform of the shapes that follow

	static void solid(int argb) {
		paint.setShader(null);
		paint.setColor(argb);
	}

	static void gradient(float x0, float y0, float x1, float y1, int[] colors, float[] positions, float opacity) {
		paint.setShader(new LinearGradient(x0, y0, x1, y1, colors, positions, Shader.TileMode.CLAMP));
		paint.setColor(0xff000000);
		paint.setAlpha(Math.round(opacity * 255));
	}

	static void transform(float a, float b, float c, float d, float e, float f) {
		identity = a == 1 && b == 0 && c == 0 && d == 1 && e == 0 && f == 0;
		values[0] = a;
		values[1] = c;
		values[2] = e;
		values[3] = b;
		values[4] = d;
		values[5] = f;
		values[6] = 0;
		values[7] = 0;
		values[8] = 1;
		matrix.setValues(values);
	}

	private static void begin(float stroke, boolean round) {
		if (!identity) {
			canvas.save();
			canvas.concat(matrix);
		}
		if (stroke > 0) {
			paint.setStyle(Paint.Style.STROKE);
			paint.setStrokeWidth(stroke);
			paint.setStrokeJoin(round ? Paint.Join.ROUND : Paint.Join.MITER);
			paint.setStrokeCap(round ? Paint.Cap.ROUND : Paint.Cap.BUTT);
			paint.setStrokeMiter(10);
		} else {
			paint.setStyle(Paint.Style.FILL);
		}
	}

	private static void end() {
		if (!identity) canvas.restore();
	}

	// ---- shapes (stroke 0: filled; else a line of that width)

	static void clear(int argb) {
		canvas.drawColor(argb, PorterDuff.Mode.SRC);
	}

	static void rect(float l, float t, float r, float b, float stroke) {
		begin(stroke, false);
		canvas.drawRect(l, t, r, b, paint);
		end();
	}

	static void rounded(float l, float t, float r, float b, float rx, float ry, float stroke) {
		begin(stroke, false);
		canvas.drawRoundRect(l, t, r, b, rx, ry, paint);
		end();
	}

	static void ellipse(float cx, float cy, float rx, float ry, float stroke) {
		begin(stroke, false);
		canvas.drawOval(cx - rx, cy - ry, cx + rx, cy + ry, paint);
		end();
	}

	static void line(float x0, float y0, float x1, float y1, float stroke) {
		begin(stroke, false);
		canvas.drawLine(x0, y0, x1, y1, paint);
		end();
	}

	static void polygon(float[] points, float stroke) {
		scratch.rewind();
		scratch.moveTo(points[0], points[1]);
		for (int i = 2; i + 1 < points.length; i += 2) scratch.lineTo(points[i], points[i + 1]);
		scratch.close();
		begin(stroke, false);
		canvas.drawPath(scratch, paint);
		end();
	}

	static void path(int id, float stroke, boolean round) {
		final Path p = paths.get(id);
		if (p == null) return;
		begin(stroke, round);
		canvas.drawPath(p, paint);
		end();
	}

	// ---- clips and layers (their rectangles are already transformed)

	static void pushClip(float l, float t, float r, float b) {
		canvas.save();
		canvas.clipRect(l, t, r, b);
	}

	static void pushOpacity(float l, float t, float r, float b, float alpha) {
		canvas.saveLayerAlpha(l, t, r, b, Math.round(alpha * 255));
	}

	static void pop() {
		canvas.restore();
	}

	// ---- paths kept by a number

	static int newPath() {
		final int id = nextId++;
		paths.put(id, new Path());
		return id;
	}

	static void pathMove(int id, float x, float y) {
		paths.get(id).moveTo(x, y);
	}

	static void pathLine(int id, float x, float y) {
		paths.get(id).lineTo(x, y);
	}

	static void pathCubic(int id, float x1, float y1, float x2, float y2, float x3, float y3) {
		paths.get(id).cubicTo(x1, y1, x2, y2, x3, y3);
	}

	static void pathClose(int id) {
		paths.get(id).close();
	}

	static void pathOval(int id, float l, float t, float r, float b) {
		paths.get(id).addOval(l, t, r, b, Path.Direction.CW);
	}

	static void freePath(int id) {
		paths.remove(id);
	}

	// ---- text

	private static Locale locale = Locale.JAPAN;

	// the language of the words (the shapes of the letters shared by Chinese and Japanese follow it)
	static void language(String tag) {
		locale = Locale.forLanguageTag(tag);
	}

	private static void font(float px, boolean isBold) {
		textPaint.setTypeface(isBold ? bold : normal);
		textPaint.setTextSize(px);
		textPaint.setTextLocale(locale);
	}

	static float textWidth(String s, float px, boolean isBold) {
		font(px, isBold);
		return textPaint.measureText(s);
	}

	// a line of text in the rectangle: align 0 left, 1 right, 2 center; in the middle of its height
	static void text(String s, float px, boolean isBold, int align, float l, float t, float r, float b, int argb, boolean clip) {
		font(px, isBold);
		textPaint.setColor(argb);
		final float w = textPaint.measureText(s);
		final Paint.FontMetrics m = textPaint.getFontMetrics();
		final float x = align == 2 ? (l + r - w) / 2 : align == 1 ? r - w : l;
		final float y = t + (b - t - (m.descent - m.ascent)) / 2 - m.ascent;
		final boolean saved = !identity || clip;
		if (saved) canvas.save();
		if (!identity) canvas.concat(matrix);
		if (clip) canvas.clipRect(l, t, r, b);
		canvas.drawText(s, x, y, textPaint);
		if (saved) canvas.restore();
	}

	// the outlines of a text in the bold font, its baseline at 0; `bounds`: left, top, right, bottom
	static int textPath(String s, float em, float[] bounds) {
		font(em, true);
		final Path p = new Path();
		textPaint.getTextPath(s, 0, s.length(), 0, 0, p);
		p.computeBounds(rect, true);
		bounds[0] = rect.left;
		bounds[1] = rect.top;
		bounds[2] = rect.right;
		bounds[3] = rect.bottom;
		final int id = nextId++;
		paths.put(id, p);
		return id;
	}

	// ---- glows: a path blurred, in a color, as a picture kept by a number (0: it could not be made)

	static int glow(int pathId, int w, int h, float originX, float originY, int argb, float sigma) {
		final Path p = paths.get(pathId);
		if (p == null) return 0;
		final Bitmap bm;
		try {
			bm = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
		} catch (RuntimeException | OutOfMemoryError e) {
			return 0;
		}
		final Canvas c = new Canvas(bm);
		final Paint gp = new Paint(Paint.ANTI_ALIAS_FLAG);
		gp.setStyle(Paint.Style.FILL);
		gp.setColor(argb);
		// (the radius of the filter for a standard deviation: sigma = 0.57735 * radius + 0.5)
		gp.setMaskFilter(new BlurMaskFilter(Math.max(0.1f, (sigma - 0.5f) / 0.57735f), BlurMaskFilter.Blur.NORMAL));
		c.translate(originX, originY);
		c.drawPath(p, gp);
		final int id = nextId++;
		glows.put(id, bm);
		return id;
	}

	static void drawGlow(int id, float x, float y, float opacity) {
		final Bitmap bm = glows.get(id);
		if (bm == null) return;
		bitmapPaint.setAlpha(Math.round(opacity * 255));
		if (!identity) {
			canvas.save();
			canvas.concat(matrix);
		}
		canvas.drawBitmap(bm, x, y, bitmapPaint);
		if (!identity) canvas.restore();
	}

	static void freeGlow(int id) {
		final Bitmap bm = glows.get(id);
		if (bm != null) bm.recycle();
		glows.remove(id);
	}
}
