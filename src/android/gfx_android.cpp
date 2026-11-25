// src/gfx.h and the Canvas of src/gfx_types.h on Android: every call draws with the Canvas of the system into a
// picture of the size of the view (the class Draw of the app, in the pixels of the screen), which end_frame puts on the
// screen. The text is drawn by the system, the shapes of the numbers are the outlines of their text, their glow is
// the blurred shape. All of it runs on the thread of the app.
#include "../gfx.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>

#include "../ini.h"
#include "../lang.h"
#include "android_bridge.h"

namespace ng {

// ---- the types of src/gfx_types.h that are our own here

// a path kept by the class Draw, by its number
class Geometry {
public:
	explicit Geometry(int i) : id(i) {}
	Geometry(const Geometry &) = delete;
	Geometry &operator=(const Geometry &) = delete;
	~Geometry();
	int id = 0;
};

class TextFormat {
public:
	TextFormat(float p, bool b, Align a) : px(p), bold(b), align(a) {}
	float px;
	bool bold;
	Align align;
};

namespace {

struct LocalFrame {
	LocalFrame() { jni()->PushLocalFrame(16); }
	~LocalFrame() { jni()->PopLocalFrame(nullptr); }
};

// a static function of Draw, found once
#define DRAW(name, signature) \
	([]() -> jmethodID {                                    \
		static const jmethodID m = draw_method(name, signature); \
		return m;                                           \
	}())

template <class... Args>
void call(jmethodID m, Args... args)
{
	jni()->CallStaticVoidMethod(draw_class(), m, args...);
}

jint argb(const Color &c, float opacity = 1)
{
	const auto ch = [](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f)); };
	return static_cast<jint>((ch(c.a * opacity) << 24) | (ch(c.r) << 16) | (ch(c.g) << 8) | ch(c.b));
}

// the brush for the shape that follows (false: nothing to draw with)
bool set_brush(Brush *b)
{
	if (auto *s = dynamic_cast<SolidBrush *>(b)) {
		call(DRAW("solid", "(I)V"), argb(s->color(), s->opacity()));
		return true;
	}
	if (auto *g = dynamic_cast<Gradient *>(b)) {
		LocalFrame frame;
		JNIEnv *e = jni();
		const auto &stops = g->stops();
		const jsize n = static_cast<jsize>(stops.size());
		std::vector<jint> colors;
		std::vector<jfloat> at;
		for (const auto &st : stops) {
			colors.push_back(argb(st.color));
			at.push_back(st.position);
		}
		const jintArray jc = e->NewIntArray(n);
		const jfloatArray ja = e->NewFloatArray(n);
		e->SetIntArrayRegion(jc, 0, n, colors.data());
		e->SetFloatArrayRegion(ja, 0, n, at.data());
		call(DRAW("gradient", "(FFFF[I[FF)V"), g->start().x, g->start().y, g->end().x, g->end().y, jc, ja, g->opacity());
		return true;
	}
	return false;
}

void set_transform_of(const Canvas *cv)
{
	const Matrix &m = cv->transform_;
	call(DRAW("transform", "(FFFFFF)V"), m._11, m._12, m._21, m._22, m._31, m._32);
}

// the rectangle that holds `r` after the transform (the clips and the layers are pushed without a rotation)
RectF transformed(const RectF &r, const Matrix &m)
{
	const float xs[4] = {r.left, r.right, r.left, r.right}, ys[4] = {r.top, r.top, r.bottom, r.bottom};
	RectF o{};
	for (int i = 0; i < 4; ++i) {
		const float x = xs[i] * m._11 + ys[i] * m._21 + m._31, y = xs[i] * m._12 + ys[i] * m._22 + m._32;
		if (i == 0 || x < o.left) o.left = x;
		if (i == 0 || x > o.right) o.right = x;
		if (i == 0 || y < o.top) o.top = y;
		if (i == 0 || y > o.bottom) o.bottom = y;
	}
	return o;
}

void polygon(const Canvas *cv, const PointF *pts, int n, Brush *b, float w)
{
	if (n <= 0 || !set_brush(b)) return;
	LocalFrame frame;
	JNIEnv *e = jni();
	std::vector<jfloat> v;
	for (int i = 0; i < n; ++i) {
		v.push_back(pts[i].x);
		v.push_back(pts[i].y);
	}
	const jfloatArray a = e->NewFloatArray(static_cast<jsize>(v.size()));
	e->SetFloatArrayRegion(a, 0, static_cast<jsize>(v.size()), v.data());
	set_transform_of(cv);
	call(DRAW("polygon", "([FF)V"), a, w);
}

int new_path() { return jni()->CallStaticIntMethod(draw_class(), DRAW("newPath", "()I")); }

std::unique_ptr<Geometry> polygon_path(const PointF *pts, int n)
{
	const int id = new_path();
	call(DRAW("pathMove", "(IFF)V"), id, pts[0].x, pts[0].y);
	for (int i = 1; i < n; ++i) call(DRAW("pathLine", "(IFF)V"), id, pts[i].x, pts[i].y);
	call(DRAW("pathClose", "(I)V"), id);
	return std::make_unique<Geometry>(id);
}

const char *language_tag()
{
	const std::wstring loc = lang::locale();
	return loc == L"ja-jp" ? "ja-JP" : loc == L"zh-cn" ? "zh-CN" : loc == L"zh-tw" ? "zh-TW" : "en-US";
}

void set_language()
{
	LocalFrame frame;
	call(DRAW("language", "(Ljava/lang/String;)V"), jni()->NewStringUTF(language_tag()));
}

}  // namespace

Geometry::~Geometry()
{
	if (id) call(DRAW("freePath", "(I)V"), id);
}

// ---- the canvas

void Canvas::clear(const Color &c) { call(DRAW("clear", "(I)V"), argb(c)); }

void Canvas::set_transform(const Matrix &m) { transform_ = m; }

void Canvas::fill_rect(const RectF &r, Brush *b) { draw_rect(r, b, 0); }

void Canvas::draw_rect(const RectF &r, Brush *b, float w)
{
	if (!set_brush(b)) return;
	set_transform_of(this);
	call(DRAW("rect", "(FFFFF)V"), r.left, r.top, r.right, r.bottom, w);
}

void Canvas::fill_rounded(const RoundRectF &r, Brush *b) { draw_rounded(r, b, 0); }

void Canvas::draw_rounded(const RoundRectF &r, Brush *b, float w)
{
	if (!set_brush(b)) return;
	set_transform_of(this);
	call(DRAW("rounded", "(FFFFFFF)V"), r.rect.left, r.rect.top, r.rect.right, r.rect.bottom, r.radiusX, r.radiusY, w);
}

void Canvas::fill_ellipse(const EllipseF &e, Brush *b) { draw_ellipse(e, b, 0); }

void Canvas::draw_ellipse(const EllipseF &e, Brush *b, float w)
{
	if (!set_brush(b)) return;
	set_transform_of(this);
	call(DRAW("ellipse", "(FFFFF)V"), e.point.x, e.point.y, e.radiusX, e.radiusY, w);
}

void Canvas::draw_line(PointF p0, PointF p1, Brush *b, float w)
{
	if (!set_brush(b)) return;
	set_transform_of(this);
	call(DRAW("line", "(FFFFF)V"), p0.x, p0.y, p1.x, p1.y, w);
}

void Canvas::fill_geometry(Geometry *g, Brush *b) { draw_geometry(g, b, 0); }

void Canvas::draw_geometry(Geometry *g, Brush *b, float w)
{
	if (!g || !set_brush(b)) return;
	set_transform_of(this);
	call(DRAW("path", "(IFZ)V"), g->id, w, static_cast<jboolean>(JNI_FALSE));
}

void Canvas::fill_polygon(const PointF *pts, int n, Brush *b) { polygon(this, pts, n, b, 0); }

void Canvas::draw_polygon(const PointF *pts, int n, Brush *b, float w) { polygon(this, pts, n, b, w); }

void Canvas::push_clip(const RectF &r)
{
	const RectF t = transformed(r, transform_);
	call(DRAW("pushClip", "(FFFF)V"), t.left, t.top, t.right, t.bottom);
}

void Canvas::pop_clip() { call(DRAW("pop", "()V")); }

void Canvas::push_opacity(const RectF &r, float a)
{
	const RectF t = transformed(r, transform_);
	call(DRAW("pushOpacity", "(FFFFF)V"), t.left, t.top, t.right, t.bottom, a);
	++layers_;
}

void Canvas::pop_opacity()
{
	call(DRAW("pop", "()V"));
	--layers_;
}

void Canvas::draw_glow(const Glow &glow, float opacity)
{
	if (!glow.image) return;
	set_transform_of(this);
	call(DRAW("drawGlow", "(IFFF)V"), *static_cast<int *>(glow.image.get()), glow.offset.x, glow.offset.y, opacity);
}

// ---- Gfx

struct Gfx::Impl {
	SolidBrush brush;
	std::map<int, std::unique_ptr<Gradient>> gradients;
	std::map<std::wstring, std::unique_ptr<TextFormat>> formats;
	std::unique_ptr<Geometry> sparkle, star, heart, ribbon, paw, rays;
	bool sized = false;
};

void Gfx::ImplDeleter::operator()(Impl *m) const { delete m; }

bool Gfx::init(NativeWindow)
{
	impl_.reset(new Impl);
	set_language();

	// unit shapes (as on Windows)
	const PointF spark[] = {{0, -0.5f}, {0.09f, -0.09f}, {0.5f, 0}, {0.09f, 0.09f},
	                        {0, 0.5f},  {-0.09f, 0.09f}, {-0.5f, 0}, {-0.09f, -0.09f}};
	impl_->sparkle = polygon_path(spark, 8);
	PointF st[10];
	for (int i = 0; i < 10; ++i) {
		const float a = static_cast<float>(-std::numbers::pi / 2 + i * std::numbers::pi / 5);
		const float r = (i & 1) ? 0.2f : 0.5f;
		st[i] = {r * std::cos(a), r * std::sin(a) + 0.04f};
	}
	impl_->star = polygon_path(st, 10);
	const PointF rib[] = {{-0.35f, -0.5f}, {0.35f, -0.5f}, {0.35f, 0.5f}, {0, 0.22f}, {-0.35f, 0.5f}};
	impl_->ribbon = polygon_path(rib, 5);
	{
		const int id = new_path();
		call(DRAW("pathMove", "(IFF)V"), id, 0.0f, 0.42f);
		call(DRAW("pathCubic", "(IFFFFFF)V"), id, -0.62f, 0.02f, -0.36f, -0.52f, 0.0f, -0.2f);
		call(DRAW("pathCubic", "(IFFFFFF)V"), id, 0.36f, -0.52f, 0.62f, 0.02f, 0.0f, 0.42f);
		call(DRAW("pathClose", "(I)V"), id);
		impl_->heart = std::make_unique<Geometry>(id);
	}
	{
		// a paw: one large pad and four toes
		const EllipseF parts[] = {
			{{0, 0.17f}, 0.27f, 0.22f},      {{-0.33f, -0.1f}, 0.1f, 0.125f}, {{-0.12f, -0.3f}, 0.105f, 0.13f},
			{{0.12f, -0.3f}, 0.105f, 0.13f}, {{0.33f, -0.1f}, 0.1f, 0.125f},
		};
		const int id = new_path();
		for (const auto &e : parts)
			call(DRAW("pathOval", "(IFFFF)V"), id, e.point.x - e.radiusX, e.point.y - e.radiusY, e.point.x + e.radiusX, e.point.y + e.radiusY);
		impl_->paw = std::make_unique<Geometry>(id);
	}
	{
		const int id = new_path();
		for (int i = 0; i < 10; ++i) {
			const double a = i * std::numbers::pi / 5, w = 0.07;
			call(DRAW("pathMove", "(IFF)V"), id, 0.0f, 0.0f);
			call(DRAW("pathLine", "(IFF)V"), id, static_cast<float>(std::cos(a - w)), static_cast<float>(std::sin(a - w)));
			call(DRAW("pathLine", "(IFF)V"), id, static_cast<float>(std::cos(a + w)), static_cast<float>(std::sin(a + w)));
			call(DRAW("pathClose", "(I)V"), id);
		}
		impl_->rays = std::make_unique<Geometry>(id);
	}
	resize();
	return true;
}

bool Gfx::ready() const noexcept { return impl_ != nullptr; }

Canvas *Gfx::dc() noexcept { return impl_ && impl_->sized ? &canvas_ : nullptr; }

Geometry *Gfx::sparkle() const { return impl_->sparkle.get(); }
Geometry *Gfx::star() const { return impl_->star.get(); }
Geometry *Gfx::heart() const { return impl_->heart.get(); }
Geometry *Gfx::ribbon() const { return impl_->ribbon.get(); }
Geometry *Gfx::paw() const { return impl_->paw.get(); }
Geometry *Gfx::rays() const { return impl_->rays.get(); }

void Gfx::resize()
{
	if (!impl_) return;
	const AndroidState &s = android_state();
	const unsigned w = static_cast<unsigned>(std::max(1, s.width)), h = static_cast<unsigned>(std::max(1, s.height));
	if (impl_->sized && w == width_ && h == height_) return;
	// (false: no picture of that size, for want of memory: nothing is drawn until a later call makes one)
	if (jni()->CallStaticBooleanMethod(draw_class(), DRAW("resize", "(II)Z"), static_cast<jint>(w), static_cast<jint>(h)) == JNI_FALSE) {
		impl_->sized = false;
		return;
	}
	impl_->sized = true;
	width_ = w;
	height_ = h;
	canvas_.cg = impl_.get();  // (not null: there is a picture to draw into)
	canvas_.transform_ = Matrix::Identity();
}

bool Gfx::begin_frame()
{
	if (!impl_ || !impl_->sized) return false;
	call(DRAW("beginFrame", "()V"));
	canvas_.transform_ = Matrix::Identity();
	drawing_ = true;
	return true;
}

void Gfx::end_frame(const std::vector<RectF> *dirty)
{
	drawing_ = false;
	canvas_.transform_ = Matrix::Identity();
	if (!impl_ || !impl_->sized || !android_state().surface) return;
	if (dirty && !dirty->empty()) {
		// (one part of the frame: the rectangle that holds the parts drawn)
		RectF all = dirty->front();
		for (const auto &r : *dirty) {
			all.left = std::min(all.left, r.left);
			all.top = std::min(all.top, r.top);
			all.right = std::max(all.right, r.right);
			all.bottom = std::max(all.bottom, r.bottom);
		}
		const jint l = std::max(0, static_cast<int>(std::floor(all.left))), t = std::max(0, static_cast<int>(std::floor(all.top)));
		const jint r = std::min(static_cast<int>(width_), static_cast<int>(std::ceil(all.right)));
		const jint b = std::min(static_cast<int>(height_), static_cast<int>(std::ceil(all.bottom)));
		if (r > l && b > t) call(DRAW("present", "(IIII)V"), l, t, r, b);
	} else {
		call(DRAW("present", "(IIII)V"), 0, 0, 0, 0);
	}
}

void *Gfx::copy_frame_image() const { return nullptr; }

SolidBrush *Gfx::brush(const Color &c)
{
	impl_->brush.SetColor(c);
	impl_->brush.SetOpacity(1.0f);
	return &impl_->brush;
}

Gradient *Gfx::gradient(int id, const GradientStop *stops, unsigned n)
{
	auto it = impl_->gradients.find(id);
	if (it != impl_->gradients.end()) return it->second.get();
	if (n == 0) return nullptr;
	auto g = std::make_unique<Gradient>(std::vector<GradientStop>(stops, stops + n));
	Gradient *p = g.get();
	impl_->gradients[id] = std::move(g);
	return p;
}

// (the system has one family of fonts for the screen: the numbers and the words are of it)
TextFormat *Gfx::text(float px, bool bold, Align align, Face)
{
	if (!(px > 0)) return nullptr;
	const std::wstring key = std::to_wstring(static_cast<int>(px * 4)) + (bold ? L"b" : L"n") + std::to_wstring(static_cast<int>(align));
	auto it = impl_->formats.find(key);
	if (it != impl_->formats.end()) return it->second.get();
	auto tf = std::make_unique<TextFormat>(px, bold, align);
	TextFormat *p = tf.get();
	impl_->formats[key] = std::move(tf);
	return p;
}

float Gfx::text_width(std::wstring_view s, TextFormat *f)
{
	if (!f || s.empty()) return 0;
	LocalFrame frame;
	return jni()->CallStaticFloatMethod(draw_class(), DRAW("textWidth", "(Ljava/lang/String;FZ)F"), to_java(s), f->px,
	                                    static_cast<jboolean>(f->bold));
}

void Gfx::draw_text(std::wstring_view s, TextFormat *f, const RectF &r, const Color &c, bool clip)
{
	if (!f || s.empty() || !canvas_.cg) return;
	LocalFrame frame;
	set_transform_of(&canvas_);
	const jint align = f->align == Align::Center ? 2 : f->align == Align::Trailing ? 1 : 0;
	call(DRAW("text", "(Ljava/lang/String;FZIFFFFIZ)V"), to_java(s), f->px, static_cast<jboolean>(f->bold), align, r.left, r.top, r.right,
	     r.bottom, argb(c), static_cast<jboolean>(clip));
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
	if (!(em > 0) || !impl_) return kNone;
	LocalFrame frame;
	JNIEnv *e = jni();
	const jfloatArray b = e->NewFloatArray(4);
	const int id = e->CallStaticIntMethod(draw_class(), DRAW("textPath", "(Ljava/lang/String;F[F)I"), to_java(text), em, b);
	if (!id) return kNone;
	jfloat v[4] = {0, 0, 0, 0};
	e->GetFloatArrayRegion(b, 0, 4, v);
	NumberShape s;
	s.bounds = {v[0], v[1], v[2], v[3]};
	s.geom = std::make_shared<Geometry>(id);
	if (shapes_.size() > 2048) shapes_.clear();
	return shapes_[key] = std::move(s);
}

void Gfx::fill_shape(const NumberShape &s, Brush *b)
{
	if (s.geom && canvas_.cg) canvas_.fill_geometry(s.geom.get(), b);
}

void Gfx::stroke_shape(const NumberShape &s, Brush *b, float stroke_w)
{
	if (!s.geom || !canvas_.cg || !set_brush(b)) return;
	set_transform_of(&canvas_);
	call(DRAW("path", "(IFZ)V"), s.geom->id, stroke_w, static_cast<jboolean>(JNI_TRUE));
}

namespace {

std::wstring glow_key(std::wstring_view shape_key, const Color &c, float radius)
{
	wchar_t b[96];
	swprintf(b, 96, L"|%02x%02x%02x%02x|%d", static_cast<int>(c.r * 255), static_cast<int>(c.g * 255), static_cast<int>(c.b * 255),
	         static_cast<int>(c.a * 255), static_cast<int>(radius * 4));
	return std::wstring(shape_key) + b;
}

// Whether the glows kept, with one more of `pixels` pixels, are too many: more than 512, or more than kGlowPixels pixels
// in all (a large window has large ones). They are then all made again as they are drawn.
constexpr double kGlowPixels = 32.0 * 1024 * 1024;

bool glows_too_many(const std::unordered_map<std::wstring, Glow> &glows, double pixels)
{
	if (glows.size() > 512) return true;
	for (const auto &[k, g] : glows) pixels += g.size.width * g.size.height;
	return pixels > kGlowPixels;
}

}  // namespace

void Gfx::prepare_glow(std::wstring_view text, float em, const Color &c, float radius, Face face)
{
	if (!impl_ || !impl_->sized || drawing_) return;
	const std::wstring key = glow_key(shape_key(text, em, face), c, radius);
	if (glows_.count(key)) return;
	const NumberShape &s = shape(text, em, face);
	if (!s.geom) return;
	const float pad = std::ceil(radius * 3.0f) + 2.0f;
	const float w = std::ceil(s.bounds.right - s.bounds.left + 2 * pad), h = std::ceil(s.bounds.bottom - s.bounds.top + 2 * pad);
	if (!(w > 0) || !(h > 0) || w > 4096 || h > 4096) return;
	const PointF origin{pad - s.bounds.left, pad - s.bounds.top};
	const int id = jni()->CallStaticIntMethod(draw_class(), DRAW("glow", "(IIIFFIF)I"), s.geom->id, static_cast<jint>(w), static_cast<jint>(h),
	                                          origin.x, origin.y, argb(c), radius);
	if (!id) return;
	if (glows_too_many(glows_, static_cast<double>(w) * h)) glows_.clear();
	Glow gl;
	gl.image = std::shared_ptr<void>(new int(id), [](void *p) {
		int *n = static_cast<int *>(p);
		call(DRAW("freeGlow", "(I)V"), *n);
		delete n;
	});
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
	if (impl_) impl_->formats.clear();
	set_language();
	clear_shapes();
}

bool Gfx::save_png(const std::wstring &path, const RectU &crop, const std::function<void()> &draw)
{
	if (!impl_ || !impl_->sized || drawing_ || width_ == 0 || height_ == 0) return false;
	if (crop.right <= crop.left || crop.bottom <= crop.top || crop.right > width_ || crop.bottom > height_) return false;
	LocalFrame frame;
	call(DRAW("beginImage", "()V"));
	canvas_.transform_ = Matrix::Identity();
	drawing_ = true;
	draw();
	drawing_ = false;
	canvas_.transform_ = Matrix::Identity();
	return jni()->CallStaticBooleanMethod(draw_class(), DRAW("endImage", "(Ljava/lang/String;IIII)Z"), to_java(path),
	                                      static_cast<jint>(crop.left), static_cast<jint>(crop.top), static_cast<jint>(crop.right),
	                                      static_cast<jint>(crop.bottom)) != JNI_FALSE;
}

}  // namespace ng
