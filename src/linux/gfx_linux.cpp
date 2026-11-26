// src/gfx.h and the Canvas of src/gfx_types.h on Linux: Cairo into a picture of the size of the drawing area (in the
// pixels of the screen, drawn in the pixels of GTK), put on the screen by the drawing area (src/linux/app_linux.cpp);
// the text with Pango, the shapes of the numbers from the outlines of their glyphs, their glow as the blurred shape.
#include <cairo.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>

#include "../gfx.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <numbers>

#include "../ini.h"
#include "../lang.h"
#include "linux_view.h"

namespace ng {

// ---- the types of src/gfx_types.h that are our own here

class Geometry {
public:
	explicit Geometry(cairo_path_t *p) : path(p) {}
	Geometry(const Geometry &) = delete;
	Geometry &operator=(const Geometry &) = delete;
	~Geometry()
	{
		if (path) cairo_path_destroy(path);
	}
	cairo_path_t *path = nullptr;
};

class TextFormat {
public:
	TextFormat(PangoFontDescription *f, Align a) : font(f), align(a) {}
	TextFormat(const TextFormat &) = delete;
	TextFormat &operator=(const TextFormat &) = delete;
	~TextFormat()
	{
		if (font) pango_font_description_free(font);
	}
	PangoFontDescription *font = nullptr;
	Align align = Align::Leading;
};

namespace {

cairo_t *C(const Canvas *c) { return static_cast<cairo_t *>(c->cg); }

// The pattern of a gradient, made once for the same points and stops (the same gradients are drawn in every frame) and
// kept; when kKeptPatterns are kept, they are all made again as they are drawn.
constexpr size_t kKeptPatterns = 256;

cairo_pattern_t *pattern_of(const Gradient &gr)
{
	static std::map<std::vector<double>, cairo_pattern_t *> kept;
	std::vector<double> key;
	for (const auto &st : gr.stops()) key.insert(key.end(), {st.position, st.color.r, st.color.g, st.color.b, st.color.a});
	const bool coincident = gr.start().x == gr.end().x && gr.start().y == gr.end().y;
	key.push_back(coincident ? 1 : 0);
	auto it = kept.find(key);
	if (it == kept.end()) {
		if (kept.size() >= kKeptPatterns) {
			for (auto &[k, p] : kept) cairo_pattern_destroy(p);
			kept.clear();
		}
		cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, coincident ? 0 : 1, 0);
		for (const auto &st : gr.stops())
			cairo_pattern_add_color_stop_rgba(p, st.position, st.color.r, st.color.g, st.color.b, st.color.a);
		cairo_pattern_set_extend(p, CAIRO_EXTEND_PAD);
		it = kept.emplace(std::move(key), p).first;
	}
	cairo_pattern_t *p = it->second;
	const double x = gr.start().x, y = gr.start().y;
	const double dx = gr.end().x - x, dy = gr.end().y - y;
	const double len2 = dx * dx + dy * dy;
	cairo_matrix_t matrix;
	if (len2 > 0)
		cairo_matrix_init(&matrix, dx / len2, -dy / len2, dy / len2, dx / len2,
		                  -(dx * x + dy * y) / len2, (dy * x - dx * y) / len2);
	else
		cairo_matrix_init_translate(&matrix, 0.5 - x, -y);
	cairo_pattern_set_matrix(p, &matrix);
	return p;
}

void apply(cairo_t *g, const Matrix &m)
{
	cairo_matrix_t cm;
	cairo_matrix_init(&cm, m._11, m._12, m._21, m._22, m._31, m._32);
	cairo_transform(g, &cm);
}

// the rectangle that holds `r` after the transform (the clips and the layers are pushed without a rotation)
void transformed(const RectF &r, const Matrix &m, double *x, double *y, double *w, double *h)
{
	const double xs[4] = {r.left, r.right, r.left, r.right}, ys[4] = {r.top, r.top, r.bottom, r.bottom};
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	for (int i = 0; i < 4; ++i) {
		const double tx = xs[i] * m._11 + ys[i] * m._21 + m._31, ty = xs[i] * m._12 + ys[i] * m._22 + m._32;
		if (i == 0 || tx < x0) x0 = tx;
		if (i == 0 || tx > x1) x1 = tx;
		if (i == 0 || ty < y0) y0 = ty;
		if (i == 0 || ty > y1) y1 = ty;
	}
	*x = x0;
	*y = y0;
	*w = x1 - x0;
	*h = y1 - y0;
}

// the opacities of the layers pushed (one canvas draws at a time)
std::vector<float> g_layers;

// a context that only makes paths and measures texts
cairo_t *scratch()
{
	static cairo_t *g = nullptr;
	if (!g) {
		cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
		g = cairo_create(s);
		cairo_surface_destroy(s);
	}
	return g;
}

// fills (stroke_w 0) or strokes the path that `add` makes, at the current transform of the canvas, with the brush
template <class Add>
void paint(const Canvas *cv, Add add, Brush *b, float stroke_w, bool round = false)
{
	if (!b || !cv->cg) return;
	cairo_t *g = C(cv);
	cairo_save(g);
	apply(g, cv->transform_);
	cairo_new_path(g);
	add(g);
	if (stroke_w > 0) {
		cairo_set_line_width(g, stroke_w);
		cairo_set_line_join(g, round ? CAIRO_LINE_JOIN_ROUND : CAIRO_LINE_JOIN_MITER);
		cairo_set_line_cap(g, round ? CAIRO_LINE_CAP_ROUND : CAIRO_LINE_CAP_BUTT);
	}
	if (auto *s = dynamic_cast<SolidBrush *>(b)) {
		const Color &c = s->color();
		cairo_set_source_rgba(g, c.r, c.g, c.b, c.a * s->opacity());
		if (stroke_w > 0) cairo_stroke(g);
		else cairo_fill(g);
	} else if (auto *gr = dynamic_cast<Gradient *>(b)) {
		cairo_pattern_t *p = pattern_of(*gr);
		if (gr->opacity() < 1) {
			cairo_push_group(g);
			cairo_set_source(g, p);
			if (stroke_w > 0) cairo_stroke(g);
			else cairo_fill(g);
			cairo_pop_group_to_source(g);
			cairo_paint_with_alpha(g, gr->opacity());
		} else {
			cairo_set_source(g, p);
			if (stroke_w > 0) cairo_stroke(g);
			else cairo_fill(g);
		}
	} else {
		cairo_new_path(g);
	}
	cairo_restore(g);
}

void add_rect(cairo_t *g, const RectF &r) { cairo_rectangle(g, r.left, r.top, r.right - r.left, r.bottom - r.top); }

void add_rounded(cairo_t *g, const RoundRectF &rr)
{
	const RectF &r = rr.rect;
	const double w = r.right - r.left, h = r.bottom - r.top;
	const double rx = std::min<double>(rr.radiusX, w / 2), ry = std::min<double>(rr.radiusY, h / 2);
	if (rx <= 0 || ry <= 0) {
		add_rect(g, r);
		return;
	}
	// four quarter ellipses (a unit arc in a scaled space)
	const double pi = std::numbers::pi;
	const struct {
		double cx, cy, a0;
	} corners[4] = {{r.right - rx, r.top + ry, -pi / 2}, {r.right - rx, r.bottom - ry, 0}, {r.left + rx, r.bottom - ry, pi / 2},
	                {r.left + rx, r.top + ry, pi}};
	cairo_new_sub_path(g);
	for (const auto &c : corners) {
		cairo_save(g);
		cairo_translate(g, c.cx, c.cy);
		cairo_scale(g, rx, ry);
		cairo_arc(g, 0, 0, 1, c.a0, c.a0 + pi / 2);
		cairo_restore(g);
	}
	cairo_close_path(g);
}

void add_ellipse(cairo_t *g, const EllipseF &e)
{
	if (!(e.radiusX > 0) || !(e.radiusY > 0)) return;
	cairo_save(g);
	cairo_translate(g, e.point.x, e.point.y);
	cairo_scale(g, e.radiusX, e.radiusY);
	cairo_new_sub_path(g);
	cairo_arc(g, 0, 0, 1, 0, 2 * std::numbers::pi);
	cairo_close_path(g);
	cairo_restore(g);
}

void add_polygon(cairo_t *g, const PointF *pts, int n)
{
	if (n <= 0) return;
	cairo_move_to(g, pts[0].x, pts[0].y);
	for (int i = 1; i < n; ++i) cairo_line_to(g, pts[i].x, pts[i].y);
	cairo_close_path(g);
}

// a path made on the scratch context, kept
template <class Add>
std::unique_ptr<Geometry> make_geometry(Add add)
{
	cairo_t *g = scratch();
	cairo_new_path(g);
	add(g);
	cairo_path_t *p = cairo_copy_path(g);
	cairo_new_path(g);
	return std::make_unique<Geometry>(p);
}

// a picture of `w` x `h` pixels of the screen, drawn in the pixels of GTK (`scale` pixels of the screen each)
cairo_surface_t *make_surface(int w, int h, double scale)
{
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(s);
		return nullptr;
	}
	cairo_surface_set_device_scale(s, scale, scale);
	return s;
}

// the families of the font of the screen, by the language of the words (the first one the system has is used)
const char *ui_family()
{
	const std::wstring loc = lang::locale();
	if (loc == L"ja-jp") return "Noto Sans CJK JP, Noto Sans JP, IPAGothic, sans-serif";
	if (loc == L"zh-cn") return "Noto Sans CJK SC, Noto Sans SC, sans-serif";
	if (loc == L"zh-tw") return "Noto Sans CJK TC, Noto Sans TC, sans-serif";
	return "sans-serif";
}

const char *pango_language()
{
	const std::wstring loc = lang::locale();
	return loc == L"ja-jp" ? "ja" : loc == L"zh-cn" ? "zh-cn" : loc == L"zh-tw" ? "zh-tw" : "en";
}

PangoFontDescription *make_font(Face face, float px, bool bold)
{
	PangoFontDescription *d = pango_font_description_new();
	pango_font_description_set_family(d, face == Face::Number ? "sans-serif" : ui_family());
	pango_font_description_set_weight(d, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
	pango_font_description_set_absolute_size(d, static_cast<double>(px) * PANGO_SCALE);
	return d;
}

// a line of text in a font, for the context `g` (released by the caller)
PangoLayout *make_layout(cairo_t *g, std::wstring_view s, const PangoFontDescription *font)
{
	PangoLayout *l = pango_cairo_create_layout(g);
	pango_context_set_language(pango_layout_get_context(l), pango_language_from_string(pango_language()));
	pango_layout_set_font_description(l, font);
	pango_layout_set_single_paragraph_mode(l, TRUE);
	const std::string u = narrow(s);
	pango_layout_set_text(l, u.c_str(), static_cast<int>(u.size()));
	return l;
}

// a blur of about a Gaussian of the standard deviation `sigma`: three box blurs, along the rows and the columns
void blur(std::vector<float> &a, int w, int h, double sigma)
{
	if (!(sigma > 0) || w <= 0 || h <= 0) return;
	const int n = 3;
	const double ideal = std::sqrt(12 * sigma * sigma / n + 1);
	int wl = static_cast<int>(std::floor(ideal));
	if (wl % 2 == 0) --wl;
	if (wl < 1) wl = 1;
	const int wu = wl + 2;
	const int m = static_cast<int>(std::lround((12 * sigma * sigma - n * wl * wl - 4 * n * wl - 3 * n) / (-4.0 * wl - 4)));
	std::vector<float> line(static_cast<size_t>(std::max(w, h)));
	for (int pass = 0; pass < n; ++pass) {
		const int r = ((pass < m ? wl : wu) - 1) / 2;
		if (r <= 0) continue;
		const float norm = 1.0f / static_cast<float>(2 * r + 1);
		auto box = [&](float *p, int count, int step) {
			for (int i = 0; i < count; ++i) line[static_cast<size_t>(i)] = p[i * step];
			double sum = 0;
			for (int i = 0; i <= r && i < count; ++i) sum += line[static_cast<size_t>(i)];
			for (int i = 0; i < count; ++i) {
				p[i * step] = static_cast<float>(sum) * norm;
				if (i + r + 1 < count) sum += line[static_cast<size_t>(i + r + 1)];
				if (i - r >= 0) sum -= line[static_cast<size_t>(i - r)];
			}
		};
		for (int y = 0; y < h; ++y) box(a.data() + static_cast<size_t>(y) * w, w, 1);
		for (int x = 0; x < w; ++x) box(a.data() + x, h, w);
	}
}

}  // namespace

// ---- the canvas

void Canvas::clear(const Color &c)
{
	cairo_t *g = C(this);
	cairo_save(g);
	cairo_set_operator(g, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_rgba(g, c.r, c.g, c.b, c.a);
	cairo_paint(g);
	cairo_restore(g);
}

void Canvas::set_transform(const Matrix &m) { transform_ = m; }

void Canvas::fill_rect(const RectF &r, Brush *b)
{
	paint(this, [&](cairo_t *g) { add_rect(g, r); }, b, 0);
}

void Canvas::draw_rect(const RectF &r, Brush *b, float w)
{
	paint(this, [&](cairo_t *g) { add_rect(g, r); }, b, w);
}

void Canvas::fill_rounded(const RoundRectF &r, Brush *b)
{
	paint(this, [&](cairo_t *g) { add_rounded(g, r); }, b, 0);
}

void Canvas::draw_rounded(const RoundRectF &r, Brush *b, float w)
{
	paint(this, [&](cairo_t *g) { add_rounded(g, r); }, b, w);
}

void Canvas::fill_ellipse(const EllipseF &e, Brush *b)
{
	paint(this, [&](cairo_t *g) { add_ellipse(g, e); }, b, 0);
}

void Canvas::draw_ellipse(const EllipseF &e, Brush *b, float w)
{
	paint(this, [&](cairo_t *g) { add_ellipse(g, e); }, b, w);
}

void Canvas::draw_line(PointF p0, PointF p1, Brush *b, float w)
{
	paint(
		this,
		[&](cairo_t *g) {
			cairo_move_to(g, p0.x, p0.y);
			cairo_line_to(g, p1.x, p1.y);
		},
		b, w);
}

void Canvas::fill_geometry(Geometry *geo, Brush *b)
{
	if (geo) paint(this, [&](cairo_t *g) { cairo_append_path(g, geo->path); }, b, 0);
}

void Canvas::draw_geometry(Geometry *geo, Brush *b, float w)
{
	if (geo) paint(this, [&](cairo_t *g) { cairo_append_path(g, geo->path); }, b, w);
}

void Canvas::fill_polygon(const PointF *pts, int n, Brush *b)
{
	paint(this, [&](cairo_t *g) { add_polygon(g, pts, n); }, b, 0);
}

void Canvas::draw_polygon(const PointF *pts, int n, Brush *b, float w)
{
	paint(this, [&](cairo_t *g) { add_polygon(g, pts, n); }, b, w);
}

void Canvas::push_clip(const RectF &r)
{
	cairo_t *g = C(this);
	double x, y, w, h;
	transformed(r, transform_, &x, &y, &w, &h);
	cairo_save(g);
	cairo_new_path(g);
	cairo_rectangle(g, x, y, w, h);
	// (no smoothing on its sides, as on Windows)
	cairo_set_antialias(g, CAIRO_ANTIALIAS_NONE);
	cairo_clip(g);
	cairo_set_antialias(g, CAIRO_ANTIALIAS_DEFAULT);
}

void Canvas::pop_clip() { cairo_restore(C(this)); }

void Canvas::push_opacity(const RectF &r, float a)
{
	cairo_t *g = C(this);
	double x, y, w, h;
	transformed(r, transform_, &x, &y, &w, &h);
	cairo_save(g);
	cairo_new_path(g);
	cairo_rectangle(g, x, y, w, h);
	cairo_set_antialias(g, CAIRO_ANTIALIAS_NONE);
	cairo_clip(g);
	cairo_set_antialias(g, CAIRO_ANTIALIAS_DEFAULT);
	cairo_push_group(g);
	g_layers.push_back(a);
	++layers_;
}

void Canvas::pop_opacity()
{
	cairo_t *g = C(this);
	const float a = g_layers.empty() ? 1.0f : g_layers.back();
	if (!g_layers.empty()) g_layers.pop_back();
	cairo_pop_group_to_source(g);
	cairo_paint_with_alpha(g, a);
	cairo_restore(g);
	--layers_;
}

void Canvas::draw_glow(const Glow &glow, float opacity)
{
	if (!glow.image) return;
	cairo_t *g = C(this);
	cairo_save(g);
	apply(g, transform_);
	// (the picture has the scale of the screen: its size here is the size it was made for)
	cairo_set_source_surface(g, static_cast<cairo_surface_t *>(glow.image.get()), glow.offset.x, glow.offset.y);
	cairo_pattern_set_filter(cairo_get_source(g), CAIRO_FILTER_BILINEAR);
	cairo_paint_with_alpha(g, opacity);
	cairo_restore(g);
}

// ---- Gfx

struct Gfx::Impl {
	GtkWidget *area = nullptr;
	cairo_surface_t *surface = nullptr;
	cairo_t *cr = nullptr;
	double scale = 1;
	int px_w = 0, px_h = 0;
	SolidBrush brush;
	std::map<int, std::unique_ptr<Gradient>> gradients;
	std::map<std::wstring, std::unique_ptr<TextFormat>> formats;
	std::unique_ptr<Geometry> sparkle, star, heart, ribbon, paw, rays;

	~Impl()
	{
		if (cr) cairo_destroy(cr);
		if (surface) cairo_surface_destroy(surface);
	}
};

void Gfx::ImplDeleter::operator()(Impl *m) const { delete m; }

bool Gfx::init(NativeWindow window)
{
	impl_.reset(new Impl);
	impl_->area = static_cast<GtkWidget *>(window);

	// unit shapes (as on Windows)
	const PointF spark[] = {{0, -0.5f}, {0.09f, -0.09f}, {0.5f, 0}, {0.09f, 0.09f},
	                        {0, 0.5f},  {-0.09f, 0.09f}, {-0.5f, 0}, {-0.09f, -0.09f}};
	impl_->sparkle = make_geometry([&](cairo_t *g) { add_polygon(g, spark, 8); });
	PointF st[10];
	for (int i = 0; i < 10; ++i) {
		const float a = static_cast<float>(-std::numbers::pi / 2 + i * std::numbers::pi / 5);
		const float r = (i & 1) ? 0.2f : 0.5f;
		st[i] = {r * std::cos(a), r * std::sin(a) + 0.04f};
	}
	impl_->star = make_geometry([&](cairo_t *g) { add_polygon(g, st, 10); });
	const PointF rib[] = {{-0.35f, -0.5f}, {0.35f, -0.5f}, {0.35f, 0.5f}, {0, 0.22f}, {-0.35f, 0.5f}};
	impl_->ribbon = make_geometry([&](cairo_t *g) { add_polygon(g, rib, 5); });
	impl_->heart = make_geometry([](cairo_t *g) {
		cairo_move_to(g, 0, 0.42);
		cairo_curve_to(g, -0.62, 0.02, -0.36, -0.52, 0, -0.2);
		cairo_curve_to(g, 0.36, -0.52, 0.62, 0.02, 0, 0.42);
		cairo_close_path(g);
	});
	impl_->paw = make_geometry([](cairo_t *g) {
		// a paw: one large pad and four toes
		const EllipseF parts[] = {
			{{0, 0.17f}, 0.27f, 0.22f},      {{-0.33f, -0.1f}, 0.1f, 0.125f}, {{-0.12f, -0.3f}, 0.105f, 0.13f},
			{{0.12f, -0.3f}, 0.105f, 0.13f}, {{0.33f, -0.1f}, 0.1f, 0.125f},
		};
		for (const auto &e : parts) add_ellipse(g, e);
	});
	impl_->rays = make_geometry([](cairo_t *g) {
		for (int i = 0; i < 10; ++i) {
			const double a = i * std::numbers::pi / 5, w = 0.07;
			cairo_move_to(g, 0, 0);
			cairo_line_to(g, std::cos(a - w), std::sin(a - w));
			cairo_line_to(g, std::cos(a + w), std::sin(a + w));
			cairo_close_path(g);
		}
	});
	resize();
	return true;
}

bool Gfx::ready() const noexcept { return impl_ && impl_->area; }

Canvas *Gfx::dc() noexcept { return impl_ && impl_->cr ? &canvas_ : nullptr; }

Geometry *Gfx::sparkle() const { return impl_->sparkle.get(); }
Geometry *Gfx::star() const { return impl_->star.get(); }
Geometry *Gfx::heart() const { return impl_->heart.get(); }
Geometry *Gfx::ribbon() const { return impl_->ribbon.get(); }
Geometry *Gfx::paw() const { return impl_->paw.get(); }
Geometry *Gfx::rays() const { return impl_->rays.get(); }

void Gfx::resize()
{
	if (!impl_ || !impl_->area) return;
	GtkWidget *area = impl_->area;
	const unsigned w = static_cast<unsigned>(std::max(1, gtk_widget_get_allocated_width(area)));
	const unsigned h = static_cast<unsigned>(std::max(1, gtk_widget_get_allocated_height(area)));
	const double scale = std::max(1, gtk_widget_get_scale_factor(area));
	if (impl_->surface && w == width_ && h == height_ && scale == impl_->scale) return;
	const int pw = static_cast<int>(std::ceil(w * scale)), ph = static_cast<int>(std::ceil(h * scale));
	cairo_surface_t *s = make_surface(pw, ph, scale);
	if (!s) return;
	if (impl_->cr) cairo_destroy(impl_->cr);
	if (impl_->surface) cairo_surface_destroy(impl_->surface);
	impl_->surface = s;
	impl_->cr = cairo_create(s);
	impl_->scale = scale;
	impl_->px_w = pw;
	impl_->px_h = ph;
	width_ = w;
	height_ = h;
	canvas_.cg = impl_->cr;
	canvas_.transform_ = Matrix::Identity();
	g_layers.clear();
}

bool Gfx::begin_frame()
{
	if (!impl_ || !impl_->cr) return false;
	canvas_.cg = impl_->cr;
	canvas_.transform_ = Matrix::Identity();
	drawing_ = true;
	return true;
}

void Gfx::end_frame(const std::vector<RectF> *dirty)
{
	drawing_ = false;
	canvas_.transform_ = Matrix::Identity();
	if (!impl_ || !impl_->surface) return;
	cairo_surface_flush(impl_->surface);
	if (in_view_draw_ || !impl_->area) return;
	if (dirty && !dirty->empty()) {
		for (const auto &r : *dirty) {
			const int x = static_cast<int>(std::floor(r.left)), y = static_cast<int>(std::floor(r.top));
			gtk_widget_queue_draw_area(impl_->area, x, y, static_cast<int>(std::ceil(r.right)) - x, static_cast<int>(std::ceil(r.bottom)) - y);
		}
	} else {
		gtk_widget_queue_draw(impl_->area);
	}
}

void *Gfx::copy_frame_image() const { return impl_ ? impl_->surface : nullptr; }

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

TextFormat *Gfx::text(float px, bool bold, Align align, Face face)
{
	if (!(px > 0)) return nullptr;
	const std::wstring key = std::to_wstring(static_cast<int>(px * 4)) + (bold ? L"b" : L"n") + std::to_wstring(static_cast<int>(align)) +
	                         (face == Face::Number ? L"N" : L"U");
	auto it = impl_->formats.find(key);
	if (it != impl_->formats.end()) return it->second.get();
	auto tf = std::make_unique<TextFormat>(make_font(face, px, bold), align);
	TextFormat *p = tf.get();
	impl_->formats[key] = std::move(tf);
	return p;
}

float Gfx::text_width(std::wstring_view s, TextFormat *f)
{
	if (!f || s.empty()) return 0;
	PangoLayout *l = make_layout(scratch(), s, f->font);
	int w = 0, h = 0;
	pango_layout_get_size(l, &w, &h);
	g_object_unref(l);
	return static_cast<float>(w) / PANGO_SCALE;
}

void Gfx::draw_text(std::wstring_view s, TextFormat *f, const RectF &r, const Color &c, bool clip)
{
	if (!f || s.empty() || !canvas_.cg) return;
	cairo_t *g = static_cast<cairo_t *>(canvas_.cg);
	cairo_save(g);
	apply(g, canvas_.transform_);
	if (clip) {
		cairo_new_path(g);
		add_rect(g, r);
		cairo_clip(g);
	}
	PangoLayout *l = make_layout(g, s, f->font);
	int pw = 0, ph = 0;
	pango_layout_get_size(l, &pw, &ph);
	const double w = static_cast<double>(pw) / PANGO_SCALE, h = static_cast<double>(ph) / PANGO_SCALE;
	double x = r.left;
	if (f->align == Align::Center) x = (r.left + r.right - w) / 2;
	else if (f->align == Align::Trailing) x = r.right - w;
	// in the middle of the rectangle, as the paragraphs of Windows
	const double y = r.top + (r.bottom - r.top - h) / 2;
	cairo_set_source_rgba(g, c.r, c.g, c.b, c.a);
	cairo_move_to(g, x, y);
	pango_cairo_show_layout(g, l);
	g_object_unref(l);
	cairo_restore(g);
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
	// the outlines of the glyphs, with the y axis down and the baseline at 0 (as the outlines of DirectWrite)
	cairo_t *g = scratch();
	PangoFontDescription *font = make_font(face, em, true);
	PangoLayout *l = make_layout(g, text, font);
	const double baseline = static_cast<double>(pango_layout_get_baseline(l)) / PANGO_SCALE;
	cairo_new_path(g);
	cairo_move_to(g, 0, -baseline);
	pango_cairo_layout_path(g, l);
	g_object_unref(l);
	pango_font_description_free(font);
	double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
	cairo_path_extents(g, &x0, &y0, &x1, &y1);
	cairo_path_t *path = cairo_copy_path(g);
	cairo_new_path(g);
	NumberShape s;
	s.bounds = {static_cast<float>(x0), static_cast<float>(y0), static_cast<float>(x1), static_cast<float>(y1)};
	s.geom = std::make_shared<Geometry>(path);
	if (shapes_.size() > 2048) shapes_.clear();
	return shapes_[key] = std::move(s);
}

void Gfx::fill_shape(const NumberShape &s, Brush *b)
{
	if (s.geom && canvas_.cg) paint(&canvas_, [&](cairo_t *g) { cairo_append_path(g, s.geom->path); }, b, 0);
}

void Gfx::stroke_shape(const NumberShape &s, Brush *b, float stroke_w)
{
	if (s.geom && canvas_.cg) paint(&canvas_, [&](cairo_t *g) { cairo_append_path(g, s.geom->path); }, b, stroke_w, true);
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

bool glows_too_many(const std::unordered_map<std::wstring, Glow> &glows, double pixels, double scale)
{
	if (glows.size() > 512) return true;
	for (const auto &[k, g] : glows) pixels += g.size.width * g.size.height * scale * scale;
	return pixels > kGlowPixels;
}

}  // namespace

void Gfx::prepare_glow(std::wstring_view text, float em, const Color &c, float radius, Face face)
{
	if (!impl_ || !impl_->cr || drawing_) return;
	const std::wstring key = glow_key(shape_key(text, em, face), c, radius);
	if (glows_.count(key)) return;
	const NumberShape &s = shape(text, em, face);
	if (!s.geom) return;
	const float pad = std::ceil(radius * 3.0f) + 2.0f;
	const float w = std::ceil(s.bounds.right - s.bounds.left + 2 * pad), h = std::ceil(s.bounds.bottom - s.bounds.top + 2 * pad);
	if (!(w > 0) || !(h > 0) || w > 4096 || h > 4096) return;
	const double scale = impl_->scale;
	const int pw = static_cast<int>(std::ceil(w * scale)), ph = static_cast<int>(std::ceil(h * scale));
	// the shape as a mask of its coverage, blurred, then in the color (as the shadow effect of Direct2D)
	cairo_surface_t *mask = cairo_image_surface_create(CAIRO_FORMAT_A8, pw, ph);
	if (cairo_surface_status(mask) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(mask);
		return;
	}
	const PointF origin{pad - s.bounds.left, pad - s.bounds.top};
	{
		cairo_t *g = cairo_create(mask);
		cairo_scale(g, scale, scale);
		cairo_translate(g, origin.x, origin.y);
		cairo_append_path(g, s.geom->path);
		cairo_set_source_rgba(g, 0, 0, 0, 1);
		cairo_fill(g);
		cairo_destroy(g);
	}
	cairo_surface_flush(mask);
	std::vector<float> a(static_cast<size_t>(pw) * ph);
	{
		const unsigned char *d = cairo_image_surface_get_data(mask);
		const int stride = cairo_image_surface_get_stride(mask);
		for (int y = 0; y < ph; ++y)
			for (int x = 0; x < pw; ++x) a[static_cast<size_t>(y) * pw + x] = d[y * stride + x] / 255.0f;
	}
	cairo_surface_destroy(mask);
	blur(a, pw, ph, radius * scale);
	cairo_surface_t *out = make_surface(pw, ph, scale);
	if (!out) return;
	{
		unsigned char *d = cairo_image_surface_get_data(out);
		const int stride = cairo_image_surface_get_stride(out);
		for (int y = 0; y < ph; ++y) {
			std::uint32_t *row = reinterpret_cast<std::uint32_t *>(d + y * stride);
			for (int x = 0; x < pw; ++x) {
				// (the colors multiplied by the opacity, as Cairo keeps them)
				const float al = std::clamp(a[static_cast<size_t>(y) * pw + x] * c.a, 0.0f, 1.0f);
				const auto ch = [&](float v) { return static_cast<std::uint32_t>(std::lround(std::clamp(v, 0.0f, 1.0f) * al * 255.0f)); };
				row[x] = (static_cast<std::uint32_t>(std::lround(al * 255.0f)) << 24) | (ch(c.r) << 16) | (ch(c.g) << 8) | ch(c.b);
			}
		}
		cairo_surface_mark_dirty(out);
	}
	if (glows_too_many(glows_, static_cast<double>(pw) * ph, scale)) glows_.clear();
	Glow gl;
	gl.image = std::shared_ptr<void>(out, [](void *p) { cairo_surface_destroy(static_cast<cairo_surface_t *>(p)); });
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
	clear_shapes();
}

bool Gfx::save_png(const std::wstring &path, const RectU &crop, const std::function<void()> &draw)
{
	if (!impl_ || !impl_->cr || drawing_ || width_ == 0 || height_ == 0) return false;
	if (crop.right <= crop.left || crop.bottom <= crop.top || crop.right > width_ || crop.bottom > height_) return false;
	cairo_surface_t *all = make_surface(impl_->px_w, impl_->px_h, impl_->scale);
	if (!all) return false;
	cairo_t *g = cairo_create(all);
	void *const old = canvas_.cg;
	canvas_.cg = g;
	canvas_.transform_ = Matrix::Identity();
	drawing_ = true;
	draw();
	drawing_ = false;
	canvas_.cg = old;
	canvas_.transform_ = Matrix::Identity();
	cairo_destroy(g);
	cairo_surface_flush(all);
	// the part asked, in the pixels of the screen
	const double sc = impl_->scale;
	const int cw = static_cast<int>(std::lround((crop.right - crop.left) * sc)), ch = static_cast<int>(std::lround((crop.bottom - crop.top) * sc));
	cairo_surface_t *part = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, cw, ch);
	bool ok = cairo_surface_status(part) == CAIRO_STATUS_SUCCESS;
	if (ok) {
		cairo_surface_set_device_scale(all, 1, 1);
		cairo_t *pg = cairo_create(part);
		cairo_set_operator(pg, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_surface(pg, all, -static_cast<double>(crop.left) * sc, -static_cast<double>(crop.top) * sc);
		cairo_paint(pg);
		cairo_destroy(pg);
		ok = cairo_surface_write_to_png(part, narrow(path).c_str()) == CAIRO_STATUS_SUCCESS;
	}
	cairo_surface_destroy(part);
	cairo_surface_destroy(all);
	return ok;
}

}  // namespace ng
