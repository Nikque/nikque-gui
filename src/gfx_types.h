// The types and the canvas of the drawing, the same names on every system. On Windows they are the Direct2D types
// (aliases) and the canvas hands every call to the Direct2D device context as it is; on macOS they are our own types,
// drawn with Core Graphics (src/mac).
#pragma once

#include <cstdint>

#if defined(_WIN32)
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#else
#include <cmath>
#include <memory>
#include <vector>
#endif

namespace ng {

#if defined(_WIN32)

using Color = D2D1_COLOR_F;
using RectF = D2D1_RECT_F;
using RectU = D2D1_RECT_U;
using PointF = D2D1_POINT_2F;
using SizeF = D2D1_SIZE_F;
using EllipseF = D2D1_ELLIPSE;
using RoundRectF = D2D1_ROUNDED_RECT;
using GradientStop = D2D1_GRADIENT_STOP;
using Matrix = D2D1::Matrix3x2F;
using Brush = ID2D1Brush;
using SolidBrush = ID2D1SolidColorBrush;
using Gradient = ID2D1LinearGradientBrush;
using Geometry = ID2D1Geometry;
using TextFormat = IDWriteTextFormat;

enum class Align : int {
	Leading = DWRITE_TEXT_ALIGNMENT_LEADING,
	Trailing = DWRITE_TEXT_ALIGNMENT_TRAILING,
	Center = DWRITE_TEXT_ALIGNMENT_CENTER,
};

#else

struct Color {
	float r, g, b, a;
};
struct RectF {
	float left, top, right, bottom;
};
struct RectU {
	std::uint32_t left, top, right, bottom;
};
struct PointF {
	float x, y;
};
struct SizeF {
	float width, height;
};
struct EllipseF {
	PointF point;
	float radiusX, radiusY;
};
struct RoundRectF {
	RectF rect;
	float radiusX, radiusY;
};
struct GradientStop {
	float position;
	Color color;
};

// a 3x2 matrix of row vectors, as Direct2D's: a * b applies a, then b; angles in degrees
struct Matrix {
	float _11 = 1, _12 = 0, _21 = 0, _22 = 1, _31 = 0, _32 = 0;

	static Matrix Identity() { return {}; }
	static Matrix Translation(float x, float y) { return {1, 0, 0, 1, x, y}; }
	static Matrix Scale(float x, float y) { return {x, 0, 0, y, 0, 0}; }
	static Matrix Rotation(float degrees)
	{
		const float a = degrees * 3.14159265358979f / 180.0f, c = std::cos(a), s = std::sin(a);
		return {c, s, -s, c, 0, 0};
	}
	Matrix operator*(const Matrix &m) const
	{
		return {_11 * m._11 + _12 * m._21,         _11 * m._12 + _12 * m._22,         _21 * m._11 + _22 * m._21,
		        _21 * m._12 + _22 * m._22,         _31 * m._11 + _32 * m._21 + m._31, _31 * m._12 + _32 * m._22 + m._32};
	}
};

// a brush: a color, or a linear gradient between two points; its opacity multiplies the colors
class Brush {
public:
	virtual ~Brush() = default;
	void SetOpacity(float a) { opacity_ = a; }
	float opacity() const { return opacity_; }

protected:
	float opacity_ = 1;
};

class SolidBrush : public Brush {
public:
	void SetColor(const Color &c) { color_ = c; }
	const Color &color() const { return color_; }

private:
	Color color_{0, 0, 0, 1};
};

class Gradient : public Brush {
public:
	explicit Gradient(std::vector<GradientStop> stops) : stops_(std::move(stops)) {}
	void SetStartPoint(PointF p) { start_ = p; }
	void SetEndPoint(PointF p) { end_ = p; }
	const std::vector<GradientStop> &stops() const { return stops_; }
	PointF start() const { return start_; }
	PointF end() const { return end_; }

private:
	std::vector<GradientStop> stops_;
	PointF start_{0, 0}, end_{0, 1};
};

class Geometry;    // a path (src/mac)
class TextFormat;  // a font, its size and the alignment (src/mac)

enum class Align : int { Leading, Trailing, Center };

#endif

constexpr RectF rect_f(float l, float t, float r, float b) { return {l, t, r, b}; }
constexpr RectU rect_u(std::uint32_t l, std::uint32_t t, std::uint32_t r, std::uint32_t b) { return {l, t, r, b}; }
constexpr EllipseF ellipse(PointF p, float rx, float ry) { return {p, rx, ry}; }
constexpr RoundRectF round_rect(const RectF &r, float rx, float ry) { return {r, rx, ry}; }

struct Glow;

// Where a frame is drawn. The rectangles, lines and shapes are drawn at the current transform; a clip and a layer of
// opacity are pushed and popped in pairs.
class Canvas {
public:
	void clear(const Color &c);
	void set_transform(const Matrix &m);
	void fill_rect(const RectF &r, Brush *b);
	void draw_rect(const RectF &r, Brush *b, float w);
	void fill_rounded(const RoundRectF &r, Brush *b);
	void draw_rounded(const RoundRectF &r, Brush *b, float w);
	void fill_ellipse(const EllipseF &e, Brush *b);
	void draw_ellipse(const EllipseF &e, Brush *b, float w);
	void draw_line(PointF p0, PointF p1, Brush *b, float w);
	void fill_geometry(Geometry *g, Brush *b);
	void draw_geometry(Geometry *g, Brush *b, float w);
	// a closed shape of straight lines
	void fill_polygon(const PointF *pts, int n, Brush *b);
	void draw_polygon(const PointF *pts, int n, Brush *b, float w);
	// a clip to a rectangle without smoothing on its sides
	void push_clip(const RectF &r);
	void pop_clip();
	// what is drawn in `r` until pop_opacity() is laid over with the opacity `a`
	void push_opacity(const RectF &r, float a);
	void pop_opacity();
	// a glow made by Gfx::prepare_glow, at the current transform (its origin is the origin of the shape)
	void draw_glow(const Glow &g, float opacity);

#if defined(_WIN32)
	ID2D1DeviceContext *d = nullptr;
	ID2D1Factory1 *factory = nullptr;
#else
	void *cg = nullptr;  // the CGContextRef of the frame
	Matrix transform_;
	int layers_ = 0;
#endif
};

#if defined(_WIN32)

inline void Canvas::clear(const Color &c) { d->Clear(c); }
inline void Canvas::set_transform(const Matrix &m) { d->SetTransform(m); }
inline void Canvas::fill_rect(const RectF &r, Brush *b) { d->FillRectangle(r, b); }
inline void Canvas::draw_rect(const RectF &r, Brush *b, float w) { d->DrawRectangle(r, b, w); }
inline void Canvas::fill_rounded(const RoundRectF &r, Brush *b) { d->FillRoundedRectangle(r, b); }
inline void Canvas::draw_rounded(const RoundRectF &r, Brush *b, float w) { d->DrawRoundedRectangle(r, b, w); }
inline void Canvas::fill_ellipse(const EllipseF &e, Brush *b) { d->FillEllipse(e, b); }
inline void Canvas::draw_ellipse(const EllipseF &e, Brush *b, float w) { d->DrawEllipse(e, b, w); }
inline void Canvas::draw_line(PointF p0, PointF p1, Brush *b, float w) { d->DrawLine(p0, p1, b, w); }
inline void Canvas::fill_geometry(Geometry *g, Brush *b) { d->FillGeometry(g, b); }
inline void Canvas::draw_geometry(Geometry *g, Brush *b, float w) { d->DrawGeometry(g, b, w); }
inline void Canvas::push_clip(const RectF &r) { d->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_ALIASED); }
inline void Canvas::pop_clip() { d->PopAxisAlignedClip(); }
inline void Canvas::push_opacity(const RectF &r, float a)
{
	d->PushLayer(D2D1::LayerParameters1(r, nullptr, D2D1_ANTIALIAS_MODE_ALIASED, D2D1::IdentityMatrix(), a), nullptr);
}
inline void Canvas::pop_opacity() { d->PopLayer(); }

namespace detail {
inline Microsoft::WRL::ComPtr<ID2D1PathGeometry> polygon(ID2D1Factory1 *f, const PointF *pts, int n)
{
	Microsoft::WRL::ComPtr<ID2D1PathGeometry> g;
	f->CreatePathGeometry(&g);
	Microsoft::WRL::ComPtr<ID2D1GeometrySink> sink;
	g->Open(&sink);
	sink->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_FILLED);
	for (int i = 1; i < n; ++i) sink->AddLine(pts[i]);
	sink->EndFigure(D2D1_FIGURE_END_CLOSED);
	sink->Close();
	return g;
}
}  // namespace detail

inline void Canvas::fill_polygon(const PointF *pts, int n, Brush *b) { d->FillGeometry(detail::polygon(factory, pts, n).Get(), b); }
inline void Canvas::draw_polygon(const PointF *pts, int n, Brush *b, float w)
{
	d->DrawGeometry(detail::polygon(factory, pts, n).Get(), b, w);
}

#endif

}  // namespace ng
