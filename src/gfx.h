// The drawing of the window. On Windows: Direct2D on a DXGI flip swap chain; on macOS: Core Graphics (src/mac). One
// pixel is one DIP (the DPI scale is applied by the layout), so the board's squares are whole pixels. Shapes of numbers
// and their glows are kept between frames.
#pragma once

#include "gfx_types.h"

#if defined(_WIN32)
#include <windows.h>

#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#endif

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ng {

#if defined(_WIN32)
using Microsoft::WRL::ComPtr;

struct NumberShape {
	ComPtr<ID2D1PathGeometry> geom;  // glyph outlines, the baseline origin at (0, 0)
	RectF bounds{};
	// the shape cut into triangles once (filling and the outline), so that a frame does not do it again
	mutable ComPtr<ID2D1GeometryRealization> fill_real, stroke_real;
	mutable float stroke_w = 0;
	bool empty() const { return !geom; }
};

struct Glow {
	ComPtr<ID2D1Bitmap1> bitmap;
	PointF offset{};  // where the bitmap's top left goes, from the shape's origin
};

inline void Canvas::draw_glow(const Glow &g, float opacity)
{
	const D2D1_SIZE_F bs = g.bitmap->GetSize();
	d->DrawBitmap(g.bitmap.Get(), D2D1::RectF(g.offset.x, g.offset.y, g.offset.x + bs.width, g.offset.y + bs.height), opacity,
	              D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
}

using NativeWindow = HWND;
#else
struct NumberShape {
	std::shared_ptr<Geometry> geom;  // glyph outlines, the baseline origin at (0, 0)
	RectF bounds{};
	bool empty() const { return !geom; }
};

struct Glow {
	std::shared_ptr<void> image;  // a CGImageRef
	PointF offset{};
	SizeF size{};
};

using NativeWindow = void *;  // the NSView
#endif

enum class Face { Number, Ui };

class Gfx {
public:
	bool init(NativeWindow window);
	void resize();
	// drawing on the back buffer; end_frame presents the whole frame, or only `dirty` when given
	bool begin_frame();
	void end_frame(const std::vector<RectF> *dirty = nullptr);

	// where to draw (null without a device)
	Canvas *dc() noexcept;
	// the window has been made ready to draw (init has been called)
	bool ready() const noexcept;
	unsigned width() const noexcept { return width_; }
	unsigned height() const noexcept { return height_; }

	SolidBrush *brush(const Color &c);
	// a linear gradient brush kept by name; its points are set by the caller (null: it could not be made)
	Gradient *gradient(int id, const GradientStop *stops, unsigned n);
	// a text format kept by its size and kind (null: it could not be made; the two below pass it by)
	TextFormat *text(float px, bool bold, Align align = Align::Leading, Face face = Face::Ui);
	// width of a text in a format (0 without one)
	float text_width(std::wstring_view s, TextFormat *f);
	void draw_text(std::wstring_view s, TextFormat *f, const RectF &r, const Color &c, bool clip = true);

	// the outline of a text (number font, bold); kept (without its geometry when the font or the geometry cannot be made:
	// not kept, drawn as nothing)
	const NumberShape &shape(std::wstring_view text, float em, Face face = Face::Number);
	// fills (or outlines with round joins, the line `stroke_w` wide) a shape at the current transform (nothing without
	// its geometry)
	void fill_shape(const NumberShape &s, Brush *b);
	void stroke_shape(const NumberShape &s, Brush *b, float stroke_w);
	// a soft glow of a shape; must be made outside begin_frame/end_frame (prepare_glow), then read with glow()
	void prepare_glow(std::wstring_view text, float em, const Color &color, float radius, Face face = Face::Number);
	const Glow *glow(std::wstring_view text, float em, const Color &color, float radius,
	                 Face face = Face::Number) const;
	// unit shapes: 4-point sparkle, 5-point star, heart (about 1 wide, centered on 0,0)
	Geometry *sparkle() const;
	Geometry *star() const;
	Geometry *heart() const;
	Geometry *ribbon() const;
	Geometry *paw() const;
	// ten thin rays from (0, 0) to radius 1
	Geometry *rays() const;

	// draws a frame into a bitmap of the window's size with `draw` and writes the part `crop` of it as a PNG file
	bool save_png(const std::wstring &path, const RectU &crop, const std::function<void()> &draw);
	void clear_shapes();  // after a size change
	void clear_fonts();   // after the language changed (the font of the screen)
#if !defined(_WIN32)
	// macOS: the picture of the frame for the view (a CGImageRef the caller releases; null: none yet)
	void *copy_frame_image() const;
	// macOS: the view is being drawn (a frame drawn meanwhile does not ask for another drawing of the view)
	void set_in_view_draw(bool on) { in_view_draw_ = on; }
#endif

private:
#if defined(_WIN32)
	bool create_device();
	void create_target();
	void release_device();
	ComPtr<IDWriteFontFace> font_face(Face face);
	std::wstring shape_key(std::wstring_view text, float em, Face face) const;

	HWND hwnd_ = nullptr;
	ComPtr<ID2D1Factory1> factory_;
	ComPtr<IDWriteFactory> dwrite_;
	ComPtr<ID3D11Device> d3d_;
	ComPtr<IDXGISwapChain1> swap_;
	ComPtr<ID2D1Device> device_;
	ComPtr<ID2D1DeviceContext> dc_;
	ComPtr<ID2D1DeviceContext1> dc1_;  // geometry realizations (null: drawn as geometries)
	ComPtr<ID2D1Bitmap1> target_;
	ComPtr<ID2D1SolidColorBrush> brush_;
	ComPtr<ID2D1StrokeStyle> round_join_;
	ComPtr<IDWriteFontFace> number_face_, ui_face_;
	ComPtr<ID2D1PathGeometry> sparkle_, star_, heart_, ribbon_;
	ComPtr<ID2D1Geometry> paw_;
	ComPtr<ID2D1PathGeometry> rays_;
	std::unordered_map<int, ComPtr<ID2D1LinearGradientBrush>> gradients_;
	std::unordered_map<std::wstring, ComPtr<IDWriteTextFormat>> formats_;
	std::unordered_map<std::wstring, NumberShape> shapes_;
	std::unordered_map<std::wstring, Glow> glows_;
#else
	struct Mac;
	struct MacDeleter {
		void operator()(Mac *m) const;
	};
	std::unique_ptr<Mac, MacDeleter> mac_;
	bool in_view_draw_ = false;
	std::wstring shape_key(std::wstring_view text, float em, Face face) const;
	std::unordered_map<std::wstring, NumberShape> shapes_;
	std::unordered_map<std::wstring, Glow> glows_;
#endif
	Canvas canvas_;
	unsigned width_ = 0, height_ = 0;
	bool full_present_needed_ = true;
	bool drawing_ = false;
};

#if defined(_WIN32)
inline Canvas *Gfx::dc() noexcept { return dc_ ? &canvas_ : nullptr; }
inline bool Gfx::ready() const noexcept { return factory_ != nullptr; }
inline Geometry *Gfx::sparkle() const { return sparkle_.Get(); }
inline Geometry *Gfx::star() const { return star_.Get(); }
inline Geometry *Gfx::heart() const { return heart_.Get(); }
inline Geometry *Gfx::ribbon() const { return ribbon_.Get(); }
inline Geometry *Gfx::paw() const { return paw_.Get(); }
inline Geometry *Gfx::rays() const { return rays_.Get(); }
#endif

}  // namespace ng
