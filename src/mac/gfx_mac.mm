// src/gfx.h and the Canvas of src/gfx_types.h on macOS: Core Graphics into a picture of the size of the view (in
// pixels of the screen, drawn in points with the y axis down), put on the screen by the view (src/mac/app_mac.mm);
// the text with Core Text, the shapes of the numbers from the outlines of their glyphs, their glow as a blurred shadow.
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <ImageIO/ImageIO.h>

#include "../gfx.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>

#include "../ini.h"
#include "../lang.h"
#include "mac_view.h"

namespace ng {

// ---- the types of src/gfx_types.h that are our own on macOS

class Geometry {
public:
	explicit Geometry(CGPathRef p) : path(p) {}
	Geometry(const Geometry &) = delete;
	Geometry &operator=(const Geometry &) = delete;
	~Geometry()
	{
		if (path) CGPathRelease(path);
	}
	CGPathRef path = nullptr;
};

class TextFormat {
public:
	TextFormat(CTFontRef f, Align a) : font(f), align(a) {}
	TextFormat(const TextFormat &) = delete;
	TextFormat &operator=(const TextFormat &) = delete;
	~TextFormat()
	{
		if (font) CFRelease(font);
	}
	CTFontRef font = nullptr;
	Align align = Align::Leading;
};

namespace {

CGContextRef C(const Canvas *c) { return static_cast<CGContextRef>(c->cg); }

CGAffineTransform T(const Matrix &m) { return CGAffineTransformMake(m._11, m._12, m._21, m._22, m._31, m._32); }

CGRect R(const RectF &r) { return CGRectMake(r.left, r.top, r.right - r.left, r.bottom - r.top); }

// the rectangle that holds `r` after the transform (the clips and the layers are pushed without a rotation)
CGRect transformed(const RectF &r, const Matrix &m) { return CGRectApplyAffineTransform(R(r), T(m)); }

CGColorRef color(const Color &c, float opacity)
{
	return CGColorCreateSRGB(c.r, c.g, c.b, c.a * opacity);
}

// a text of wide letters (code points) for Core Text
CFStringRef cfstr(std::wstring_view s)
{
	return CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8 *>(s.data()), static_cast<CFIndex>(s.size() * sizeof(wchar_t)),
	                               kCFStringEncodingUTF32LE, false);
}

CTLineRef make_line(std::wstring_view s, CTFontRef font, CGColorRef fg)
{
	CFStringRef str = cfstr(s);
	if (!str) return nullptr;
	const void *keys[2] = {kCTFontAttributeName, kCTForegroundColorAttributeName};
	const void *values[2] = {font, fg};
	CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, values, fg ? 2 : 1, &kCFTypeDictionaryKeyCallBacks,
	                                           &kCFTypeDictionaryValueCallBacks);
	CFAttributedStringRef as = CFAttributedStringCreate(nullptr, str, attrs);
	CTLineRef line = CTLineCreateWithAttributedString(as);
	CFRelease(as);
	CFRelease(attrs);
	CFRelease(str);
	return line;
}

// fills (stroke_w 0) or strokes the path at the current transform of the canvas with the brush
void paint(const Canvas *cv, CGPathRef path, Brush *b, float stroke_w, bool round = false)
{
	if (!path || !b) return;
	CGContextRef g = C(cv);
	CGContextSaveGState(g);
	CGContextConcatCTM(g, T(cv->transform_));
	CGContextAddPath(g, path);
	if (stroke_w > 0) {
		CGContextSetLineWidth(g, stroke_w);
		CGContextSetLineJoin(g, round ? kCGLineJoinRound : kCGLineJoinMiter);
		CGContextSetLineCap(g, round ? kCGLineCapRound : kCGLineCapButt);
	}
	if (auto *s = dynamic_cast<SolidBrush *>(b)) {
		CGColorRef c = color(s->color(), s->opacity());
		if (stroke_w > 0) {
			CGContextSetStrokeColorWithColor(g, c);
			CGContextStrokePath(g);
		} else {
			CGContextSetFillColorWithColor(g, c);
			CGContextFillPath(g);
		}
		CGColorRelease(c);
	} else if (auto *gr = dynamic_cast<Gradient *>(b)) {
		if (stroke_w > 0) CGContextReplacePathWithStrokedPath(g);
		CGContextClip(g);
		const auto &stops = gr->stops();
		std::vector<CGFloat> comps, locs;
		for (const auto &st : stops) {
			comps.insert(comps.end(), {st.color.r, st.color.g, st.color.b, st.color.a});
			locs.push_back(st.position);
		}
		CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
		CGGradientRef grad = CGGradientCreateWithColorComponents(cs, comps.data(), locs.data(), locs.size());
		CGContextSetAlpha(g, gr->opacity());
		CGContextDrawLinearGradient(g, grad, CGPointMake(gr->start().x, gr->start().y), CGPointMake(gr->end().x, gr->end().y),
		                            kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
		CGGradientRelease(grad);
		CGColorSpaceRelease(cs);
	}
	CGContextRestoreGState(g);
}

CGPathRef polygon_path(const PointF *pts, int n)
{
	CGMutablePathRef p = CGPathCreateMutable();
	CGPathMoveToPoint(p, nullptr, pts[0].x, pts[0].y);
	for (int i = 1; i < n; ++i) CGPathAddLineToPoint(p, nullptr, pts[i].x, pts[i].y);
	CGPathCloseSubpath(p);
	return p;
}

CGPathRef rounded_path(const RoundRectF &r)
{
	const CGRect rc = R(r.rect);
	const CGFloat rx = std::min<CGFloat>(r.radiusX, rc.size.width / 2), ry = std::min<CGFloat>(r.radiusY, rc.size.height / 2);
	if (rx <= 0 || ry <= 0) return CGPathCreateWithRect(rc, nullptr);
	return CGPathCreateWithRoundedRect(rc, rx, ry, nullptr);
}

CGPathRef ellipse_path(const EllipseF &e)
{
	return CGPathCreateWithEllipseInRect(
		CGRectMake(e.point.x - e.radiusX, e.point.y - e.radiusY, 2 * e.radiusX, 2 * e.radiusY), nullptr);
}

// a bitmap of `w` x `h` pixels in sRGB, drawn in points (`scale` pixels each) with the y axis down
CGContextRef make_bitmap(size_t w, size_t h, CGFloat scale)
{
	CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGContextRef g = CGBitmapContextCreate(nullptr, w, h, 8, 0, cs,
	                                       kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
	CGColorSpaceRelease(cs);
	if (!g) return nullptr;
	CGContextTranslateCTM(g, 0, static_cast<CGFloat>(h));
	CGContextScaleCTM(g, scale, -scale);
	CGContextSetTextMatrix(g, CGAffineTransformMakeScale(1, -1));
	return g;
}

}  // namespace

// ---- the canvas

void Canvas::clear(const Color &c)
{
	CGContextRef g = C(this);
	CGContextSaveGState(g);
	const CGRect all = CGContextGetClipBoundingBox(g);
	CGContextClearRect(g, all);
	if (c.a > 0) {
		CGColorRef col = color(c, 1);
		CGContextSetFillColorWithColor(g, col);
		CGContextFillRect(g, all);
		CGColorRelease(col);
	}
	CGContextRestoreGState(g);
}

void Canvas::set_transform(const Matrix &m) { transform_ = m; }

void Canvas::fill_rect(const RectF &r, Brush *b)
{
	CGPathRef p = CGPathCreateWithRect(R(r), nullptr);
	paint(this, p, b, 0);
	CGPathRelease(p);
}

void Canvas::draw_rect(const RectF &r, Brush *b, float w)
{
	CGPathRef p = CGPathCreateWithRect(R(r), nullptr);
	paint(this, p, b, w);
	CGPathRelease(p);
}

void Canvas::fill_rounded(const RoundRectF &r, Brush *b)
{
	CGPathRef p = rounded_path(r);
	paint(this, p, b, 0);
	CGPathRelease(p);
}

void Canvas::draw_rounded(const RoundRectF &r, Brush *b, float w)
{
	CGPathRef p = rounded_path(r);
	paint(this, p, b, w);
	CGPathRelease(p);
}

void Canvas::fill_ellipse(const EllipseF &e, Brush *b)
{
	CGPathRef p = ellipse_path(e);
	paint(this, p, b, 0);
	CGPathRelease(p);
}

void Canvas::draw_ellipse(const EllipseF &e, Brush *b, float w)
{
	CGPathRef p = ellipse_path(e);
	paint(this, p, b, w);
	CGPathRelease(p);
}

void Canvas::draw_line(PointF p0, PointF p1, Brush *b, float w)
{
	CGMutablePathRef p = CGPathCreateMutable();
	CGPathMoveToPoint(p, nullptr, p0.x, p0.y);
	CGPathAddLineToPoint(p, nullptr, p1.x, p1.y);
	paint(this, p, b, w);
	CGPathRelease(p);
}

void Canvas::fill_geometry(Geometry *g, Brush *b)
{
	if (g) paint(this, g->path, b, 0);
}

void Canvas::draw_geometry(Geometry *g, Brush *b, float w)
{
	if (g) paint(this, g->path, b, w);
}

void Canvas::fill_polygon(const PointF *pts, int n, Brush *b)
{
	CGPathRef p = polygon_path(pts, n);
	paint(this, p, b, 0);
	CGPathRelease(p);
}

void Canvas::draw_polygon(const PointF *pts, int n, Brush *b, float w)
{
	CGPathRef p = polygon_path(pts, n);
	paint(this, p, b, w);
	CGPathRelease(p);
}

void Canvas::push_clip(const RectF &r)
{
	CGContextRef g = C(this);
	CGContextSaveGState(g);
	CGContextClipToRect(g, transformed(r, transform_));
}

void Canvas::pop_clip() { CGContextRestoreGState(C(this)); }

void Canvas::push_opacity(const RectF &r, float a)
{
	CGContextRef g = C(this);
	CGContextSaveGState(g);
	CGContextSetAlpha(g, a);
	CGContextBeginTransparencyLayerWithRect(g, transformed(r, transform_), nullptr);
	++layers_;
}

void Canvas::pop_opacity()
{
	CGContextRef g = C(this);
	CGContextEndTransparencyLayer(g);
	CGContextRestoreGState(g);
	--layers_;
}

void Canvas::draw_glow(const Glow &glow, float opacity)
{
	if (!glow.image) return;
	CGContextRef g = C(this);
	CGContextSaveGState(g);
	CGContextConcatCTM(g, T(transform_));
	CGContextSetAlpha(g, opacity);
	CGContextSetInterpolationQuality(g, kCGInterpolationHigh);
	// (the picture is upright in a context whose y axis goes up: turned for ours)
	const CGRect rc = CGRectMake(glow.offset.x, glow.offset.y, glow.size.width, glow.size.height);
	CGContextTranslateCTM(g, 0, CGRectGetMinY(rc) + CGRectGetMaxY(rc));
	CGContextScaleCTM(g, 1, -1);
	CGContextDrawImage(g, rc, static_cast<CGImageRef>(glow.image.get()));
	CGContextRestoreGState(g);
}

// ---- Gfx

struct Gfx::Mac {
	__weak NgView *view = nil;
	CGContextRef bitmap = nullptr;
	CGFloat scale = 1;
	size_t px_w = 0, px_h = 0;
	SolidBrush brush;
	std::map<int, std::unique_ptr<Gradient>> gradients;
	std::map<std::wstring, std::unique_ptr<TextFormat>> formats;
	std::unique_ptr<Geometry> sparkle, star, heart, ribbon, paw, rays;

	~Mac()
	{
		if (bitmap) CGContextRelease(bitmap);
	}
};

void Gfx::MacDeleter::operator()(Mac *m) const { delete m; }

namespace {

CTFontRef make_font(Face face, float px, bool bold)
{
	CTFontRef base = nullptr;
	if (face == Face::Number) {
		base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, px, nullptr);
	} else {
		CFStringRef name = cfstr(lang::ui_font());
		base = CTFontCreateWithName(name, px, nullptr);
		CFRelease(name);
	}
	if (!base) return nullptr;
	if (!bold) return base;
	CTFontRef b = CTFontCreateCopyWithSymbolicTraits(base, px, nullptr, kCTFontBoldTrait, kCTFontBoldTrait);
	if (!b) return base;
	CFRelease(base);
	return b;
}

std::unique_ptr<Geometry> poly(const PointF *pts, int n) { return std::make_unique<Geometry>(polygon_path(pts, n)); }

}  // namespace

bool Gfx::init(NativeWindow window)
{
	mac_.reset(new Mac);
	mac_->view = (__bridge NgView *)window;

	// unit shapes (as on Windows)
	const PointF spark[] = {{0, -0.5f}, {0.09f, -0.09f}, {0.5f, 0}, {0.09f, 0.09f},
	                        {0, 0.5f},  {-0.09f, 0.09f}, {-0.5f, 0}, {-0.09f, -0.09f}};
	mac_->sparkle = poly(spark, 8);
	PointF st[10];
	for (int i = 0; i < 10; ++i) {
		const float a = static_cast<float>(-std::numbers::pi / 2 + i * std::numbers::pi / 5);
		const float r = (i & 1) ? 0.2f : 0.5f;
		st[i] = {r * std::cos(a), r * std::sin(a) + 0.04f};
	}
	mac_->star = poly(st, 10);
	const PointF rib[] = {{-0.35f, -0.5f}, {0.35f, -0.5f}, {0.35f, 0.5f}, {0, 0.22f}, {-0.35f, 0.5f}};
	mac_->ribbon = poly(rib, 5);
	{
		CGMutablePathRef p = CGPathCreateMutable();
		CGPathMoveToPoint(p, nullptr, 0, 0.42f);
		CGPathAddCurveToPoint(p, nullptr, -0.62f, 0.02f, -0.36f, -0.52f, 0, -0.2f);
		CGPathAddCurveToPoint(p, nullptr, 0.36f, -0.52f, 0.62f, 0.02f, 0, 0.42f);
		CGPathCloseSubpath(p);
		mac_->heart = std::make_unique<Geometry>(p);
	}
	{
		// a paw: one large pad and four toes
		const EllipseF parts[] = {
			{{0, 0.17f}, 0.27f, 0.22f},    {{-0.33f, -0.1f}, 0.1f, 0.125f}, {{-0.12f, -0.3f}, 0.105f, 0.13f},
			{{0.12f, -0.3f}, 0.105f, 0.13f}, {{0.33f, -0.1f}, 0.1f, 0.125f},
		};
		CGMutablePathRef p = CGPathCreateMutable();
		for (const auto &e : parts)
			CGPathAddEllipseInRect(p, nullptr, CGRectMake(e.point.x - e.radiusX, e.point.y - e.radiusY, 2 * e.radiusX, 2 * e.radiusY));
		mac_->paw = std::make_unique<Geometry>(p);
	}
	{
		CGMutablePathRef p = CGPathCreateMutable();
		for (int i = 0; i < 10; ++i) {
			const double a = i * std::numbers::pi / 5, w = 0.07;
			CGPathMoveToPoint(p, nullptr, 0, 0);
			CGPathAddLineToPoint(p, nullptr, std::cos(a - w), std::sin(a - w));
			CGPathAddLineToPoint(p, nullptr, std::cos(a + w), std::sin(a + w));
			CGPathCloseSubpath(p);
		}
		mac_->rays = std::make_unique<Geometry>(p);
	}
	resize();
	return true;
}

bool Gfx::ready() const noexcept { return mac_ && mac_->view != nil; }

Canvas *Gfx::dc() noexcept { return mac_ && mac_->bitmap ? &canvas_ : nullptr; }

Geometry *Gfx::sparkle() const { return mac_->sparkle.get(); }
Geometry *Gfx::star() const { return mac_->star.get(); }
Geometry *Gfx::heart() const { return mac_->heart.get(); }
Geometry *Gfx::ribbon() const { return mac_->ribbon.get(); }
Geometry *Gfx::paw() const { return mac_->paw.get(); }
Geometry *Gfx::rays() const { return mac_->rays.get(); }

void Gfx::resize()
{
	if (!mac_) return;
	NgView *v = mac_->view;
	if (!v) return;
	const NSSize s = v.bounds.size;
	const CGFloat scale = v.window ? v.window.backingScaleFactor : NSScreen.mainScreen.backingScaleFactor;
	const unsigned w = std::max(1u, static_cast<unsigned>(std::lround(s.width)));
	const unsigned h = std::max(1u, static_cast<unsigned>(std::lround(s.height)));
	if (mac_->bitmap && w == width_ && h == height_ && scale == mac_->scale) return;
	const size_t pw = static_cast<size_t>(std::ceil(w * scale)), ph = static_cast<size_t>(std::ceil(h * scale));
	CGContextRef g = make_bitmap(pw, ph, scale);
	if (!g) return;
	if (mac_->bitmap) CGContextRelease(mac_->bitmap);
	mac_->bitmap = g;
	mac_->scale = scale;
	mac_->px_w = pw;
	mac_->px_h = ph;
	width_ = w;
	height_ = h;
	canvas_.cg = g;
	canvas_.transform_ = Matrix::Identity();
}

bool Gfx::begin_frame()
{
	if (!mac_ || !mac_->bitmap) return false;
	canvas_.cg = mac_->bitmap;
	canvas_.transform_ = Matrix::Identity();
	drawing_ = true;
	return true;
}

void Gfx::end_frame(const std::vector<RectF> *dirty)
{
	drawing_ = false;
	canvas_.transform_ = Matrix::Identity();
	if (in_view_draw_ || !mac_) return;
	NgView *v = mac_->view;
	if (!v) return;
	if (dirty && !dirty->empty()) {
		for (const auto &r : *dirty) [v setNeedsDisplayInRect:NSMakeRect(r.left, r.top, r.right - r.left, r.bottom - r.top)];
	} else {
		v.needsDisplay = YES;
	}
}

void *Gfx::copy_frame_image() const
{
	if (!mac_ || !mac_->bitmap) return nullptr;
	return CGBitmapContextCreateImage(mac_->bitmap);
}

SolidBrush *Gfx::brush(const Color &c)
{
	mac_->brush.SetColor(c);
	mac_->brush.SetOpacity(1.0f);
	return &mac_->brush;
}

Gradient *Gfx::gradient(int id, const GradientStop *stops, unsigned n)
{
	auto it = mac_->gradients.find(id);
	if (it != mac_->gradients.end()) return it->second.get();
	if (n == 0) return nullptr;
	auto g = std::make_unique<Gradient>(std::vector<GradientStop>(stops, stops + n));
	Gradient *p = g.get();
	mac_->gradients[id] = std::move(g);
	return p;
}

TextFormat *Gfx::text(float px, bool bold, Align align, Face face)
{
	if (!(px > 0)) return nullptr;
	const std::wstring key = std::to_wstring(static_cast<int>(px * 4)) + (bold ? L"b" : L"n") + std::to_wstring(static_cast<int>(align)) +
	                         (face == Face::Number ? L"N" : L"U");
	auto it = mac_->formats.find(key);
	if (it != mac_->formats.end()) return it->second.get();
	CTFontRef f = make_font(face, px, bold);
	if (!f) return nullptr;
	auto tf = std::make_unique<TextFormat>(f, align);
	TextFormat *p = tf.get();
	mac_->formats[key] = std::move(tf);
	return p;
}

float Gfx::text_width(std::wstring_view s, TextFormat *f)
{
	if (!f || s.empty()) return 0;
	CTLineRef line = make_line(s, f->font, nullptr);
	if (!line) return 0;
	const double w = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
	CFRelease(line);
	return static_cast<float>(w);
}

void Gfx::draw_text(std::wstring_view s, TextFormat *f, const RectF &r, const Color &c, bool clip)
{
	if (!f || s.empty() || !canvas_.cg) return;
	CGColorRef col = color(c, 1);
	CTLineRef line = make_line(s, f->font, col);
	CGColorRelease(col);
	if (!line) return;
	CGFloat ascent = 0, descent = 0, leading = 0;
	const double w = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
	CGFloat x = r.left;
	if (f->align == Align::Center) x = (r.left + r.right - w) / 2;
	else if (f->align == Align::Trailing) x = r.right - w;
	// in the middle of the rectangle, as the paragraphs of Windows
	const CGFloat baseline = r.top + (r.bottom - r.top - (ascent + descent)) / 2 + ascent;
	CGContextRef g = static_cast<CGContextRef>(canvas_.cg);
	CGContextSaveGState(g);
	CGContextConcatCTM(g, T(canvas_.transform_));
	if (clip) CGContextClipToRect(g, R(r));
	CGContextSetTextMatrix(g, CGAffineTransformMakeScale(1, -1));
	CGContextSetTextPosition(g, x, baseline);
	CTLineDraw(line, g);
	CGContextRestoreGState(g);
	CFRelease(line);
}

std::wstring Gfx::shape_key(std::wstring_view text, float em, Face face) const
{
	return std::wstring(text) + L'|' + std::to_wstring(static_cast<int>(em * 2)) + (face == Face::Number ? L"N" : L"U");
}

const NumberShape &Gfx::shape(std::wstring_view text, float em, Face face)
{
	const std::wstring key = shape_key(text, em, face);
	auto it = shapes_.find(key);
	if (it != shapes_.end()) return it->second;
	static const NumberShape kNone{};
	if (!(em > 0)) return kNone;
	CTFontRef font = make_font(face, em, true);
	if (!font) return kNone;
	CTLineRef line = make_line(text, font, nullptr);
	CFRelease(font);
	if (!line) return kNone;
	// the outlines of the glyphs, with the y axis down and the baseline at 0 (as the outlines of DirectWrite)
	CGMutablePathRef path = CGPathCreateMutable();
	CFArrayRef runs = CTLineGetGlyphRuns(line);
	for (CFIndex i = 0; i < CFArrayGetCount(runs); ++i) {
		CTRunRef run = static_cast<CTRunRef>(CFArrayGetValueAtIndex(runs, i));
		CTFontRef rf = static_cast<CTFontRef>(CFDictionaryGetValue(CTRunGetAttributes(run), kCTFontAttributeName));
		const CFIndex n = CTRunGetGlyphCount(run);
		std::vector<CGGlyph> glyphs(static_cast<size_t>(n));
		std::vector<CGPoint> pos(static_cast<size_t>(n));
		CTRunGetGlyphs(run, CFRangeMake(0, 0), glyphs.data());
		CTRunGetPositions(run, CFRangeMake(0, 0), pos.data());
		for (CFIndex k = 0; k < n; ++k) {
			const CGAffineTransform m = CGAffineTransformMake(1, 0, 0, -1, pos[k].x, -pos[k].y);
			if (CGPathRef gp = CTFontCreatePathForGlyph(rf, glyphs[k], &m)) {
				CGPathAddPath(path, nullptr, gp);
				CGPathRelease(gp);
			}
		}
	}
	CFRelease(line);
	NumberShape s;
	const CGRect b = CGPathIsEmpty(path) ? CGRectZero : CGPathGetPathBoundingBox(path);
	s.bounds = {static_cast<float>(CGRectGetMinX(b)), static_cast<float>(CGRectGetMinY(b)), static_cast<float>(CGRectGetMaxX(b)),
	            static_cast<float>(CGRectGetMaxY(b))};
	s.geom = std::make_shared<Geometry>(path);
	if (shapes_.size() > 2048) shapes_.clear();
	return shapes_[key] = std::move(s);
}

void Gfx::fill_shape(const NumberShape &s, Brush *b)
{
	if (s.geom && canvas_.cg) paint(&canvas_, s.geom->path, b, 0);
}

void Gfx::stroke_shape(const NumberShape &s, Brush *b, float stroke_w)
{
	if (s.geom && canvas_.cg) paint(&canvas_, s.geom->path, b, stroke_w, true);
}

namespace {

std::wstring glow_key(std::wstring_view shape_key, const Color &c, float radius)
{
	wchar_t b[96];
	swprintf(b, 96, L"|%02x%02x%02x%02x|%d", static_cast<int>(c.r * 255), static_cast<int>(c.g * 255), static_cast<int>(c.b * 255),
	         static_cast<int>(c.a * 255), static_cast<int>(radius * 4));
	return std::wstring(shape_key) + b;
}

}  // namespace

void Gfx::prepare_glow(std::wstring_view text, float em, const Color &c, float radius, Face face)
{
	if (!mac_ || !mac_->bitmap || drawing_) return;
	const std::wstring key = glow_key(shape_key(text, em, face), c, radius);
	if (glows_.count(key)) return;
	const NumberShape &s = shape(text, em, face);
	if (!s.geom) return;
	const float pad = std::ceil(radius * 3.0f) + 2.0f;
	const float w = std::ceil(s.bounds.right - s.bounds.left + 2 * pad), h = std::ceil(s.bounds.bottom - s.bounds.top + 2 * pad);
	if (!(w > 0) || !(h > 0) || w > 4096 || h > 4096) return;
	const CGFloat scale = mac_->scale;
	CGContextRef g = make_bitmap(static_cast<size_t>(std::ceil(w * scale)), static_cast<size_t>(std::ceil(h * scale)), scale);
	if (!g) return;
	const PointF origin{pad - s.bounds.left, pad - s.bounds.top};
	// the shape in the color with its shadow (the blurred shape) on it: the shape itself is under the number, which hides
	// it (Quartz draws no shadow for a shape outside the picture)
	CGColorRef col = color(c, 1);
	CGContextSetShadowWithColor(g, CGSizeZero, 2 * radius * scale, col);
	CGContextTranslateCTM(g, origin.x, origin.y);
	CGContextAddPath(g, s.geom->path);
	CGContextSetFillColorWithColor(g, col);
	CGContextFillPath(g);
	CGColorRelease(col);
	CGImageRef img = CGBitmapContextCreateImage(g);
	CGContextRelease(g);
	if (!img) return;
	if (glows_.size() > 512) glows_.clear();
	Glow gl;
	gl.image = std::shared_ptr<void>(img, [](void *p) { CGImageRelease(static_cast<CGImageRef>(p)); });
	gl.offset = {-origin.x, -origin.y};
	gl.size = {w, h};
	glows_[key] = std::move(gl);
}

const Glow *Gfx::glow(std::wstring_view text, float em, const Color &c, float radius, Face face) const
{
	auto it = glows_.find(glow_key(shape_key(text, em, face), c, radius));
	return it == glows_.end() ? nullptr : &it->second;
}

void Gfx::clear_shapes()
{
	shapes_.clear();
	glows_.clear();
}

void Gfx::clear_fonts()
{
	if (mac_) mac_->formats.clear();
	clear_shapes();
}

bool Gfx::save_png(const std::wstring &path, const RectU &crop, const std::function<void()> &draw)
{
	if (!mac_ || !mac_->bitmap || drawing_ || width_ == 0 || height_ == 0) return false;
	if (crop.right <= crop.left || crop.bottom <= crop.top || crop.right > width_ || crop.bottom > height_) return false;
	CGContextRef g = make_bitmap(mac_->px_w, mac_->px_h, mac_->scale);
	if (!g) return false;
	void *const old = canvas_.cg;
	canvas_.cg = g;
	canvas_.transform_ = Matrix::Identity();
	drawing_ = true;
	draw();
	drawing_ = false;
	canvas_.cg = old;
	canvas_.transform_ = Matrix::Identity();
	CGImageRef all = CGBitmapContextCreateImage(g);
	CGContextRelease(g);
	if (!all) return false;
	const CGFloat sc = mac_->scale;
	CGImageRef part = CGImageCreateWithImageInRect(
		all, CGRectMake(crop.left * sc, crop.top * sc, (crop.right - crop.left) * sc, (crop.bottom - crop.top) * sc));
	CGImageRelease(all);
	if (!part) return false;
	NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:narrow(path).c_str()]];
	CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
	bool ok = false;
	if (dest) {
		CGImageDestinationAddImage(dest, part, nullptr);
		ok = CGImageDestinationFinalize(dest);
		CFRelease(dest);
	}
	CGImageRelease(part);
	return ok;
}

}  // namespace ng
