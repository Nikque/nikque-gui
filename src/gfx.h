// Direct2D on a DXGI flip swap chain. One pixel is one DIP (the DPI scale is applied by the layout), so the
// board's squares are whole pixels. Shapes of numbers and their glows are kept between frames.
#pragma once

#include <d2d1_1.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ng {

using Microsoft::WRL::ComPtr;

struct NumberShape {
	ComPtr<ID2D1PathGeometry> geom;  // glyph outlines, the baseline origin at (0, 0)
	D2D1_RECT_F bounds{};
};

struct Glow {
	ComPtr<ID2D1Bitmap1> bitmap;
	D2D1_POINT_2F offset{};  // where the bitmap's top left goes, from the shape's origin
};

enum class Face { Number, Ui };

class Gfx {
public:
	bool init(HWND hwnd);
	void resize();
	// drawing on the back buffer; end_frame presents the whole frame, or only `dirty` when given
	bool begin_frame();
	void end_frame(const std::vector<D2D1_RECT_F> *dirty = nullptr);

	ID2D1DeviceContext *dc() const noexcept { return dc_.Get(); }
	ID2D1Factory1 *factory() const noexcept { return factory_.Get(); }
	IDWriteFactory *dwrite() const noexcept { return dwrite_.Get(); }
	UINT width() const noexcept { return width_; }
	UINT height() const noexcept { return height_; }

	ID2D1SolidColorBrush *brush(const D2D1_COLOR_F &c);
	// a linear gradient brush kept by name; its points are set by the caller
	ID2D1LinearGradientBrush *gradient(int id, const D2D1_GRADIENT_STOP *stops, UINT n);
	IDWriteTextFormat *text(float px, bool bold, DWRITE_TEXT_ALIGNMENT align = DWRITE_TEXT_ALIGNMENT_LEADING,
	                        Face face = Face::Ui);
	// width of a text in a format
	float text_width(std::wstring_view s, IDWriteTextFormat *f);
	void draw_text(std::wstring_view s, IDWriteTextFormat *f, const D2D1_RECT_F &r, const D2D1_COLOR_F &c,
	               bool clip = true);

	// the outline of a text (number font, bold); kept
	const NumberShape &shape(std::wstring_view text, float em, Face face = Face::Number);
	// a soft glow of a shape; must be made outside begin_frame/end_frame (prepare_glow), then read with glow()
	void prepare_glow(std::wstring_view text, float em, const D2D1_COLOR_F &color, float radius, Face face = Face::Number);
	const Glow *glow(std::wstring_view text, float em, const D2D1_COLOR_F &color, float radius,
	                 Face face = Face::Number) const;
	// unit shapes: 4-point sparkle, 5-point star, heart (about 1 wide, centered on 0,0)
	ID2D1PathGeometry *sparkle() const { return sparkle_.Get(); }
	ID2D1PathGeometry *star() const { return star_.Get(); }
	ID2D1PathGeometry *heart() const { return heart_.Get(); }
	ID2D1Geometry *paw() const { return paw_.Get(); }
	// ten thin rays from (0, 0) to radius 1
	ID2D1PathGeometry *rays() const { return rays_.Get(); }
	ID2D1StrokeStyle *round_join() const { return round_join_.Get(); }

	void clear_shapes();  // after a size change
	size_t glow_count() const noexcept { return glows_.size(); }
	size_t shape_count() const noexcept { return shapes_.size(); }

private:
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
	ComPtr<ID2D1Bitmap1> target_;
	ComPtr<ID2D1SolidColorBrush> brush_;
	ComPtr<ID2D1StrokeStyle> round_join_;
	ComPtr<IDWriteFontFace> number_face_, ui_face_;
	ComPtr<ID2D1PathGeometry> sparkle_, star_, heart_;
	ComPtr<ID2D1Geometry> paw_;
	ComPtr<ID2D1PathGeometry> rays_;
	std::unordered_map<int, ComPtr<ID2D1LinearGradientBrush>> gradients_;
	std::unordered_map<std::wstring, ComPtr<IDWriteTextFormat>> formats_;
	std::unordered_map<std::wstring, NumberShape> shapes_;
	std::unordered_map<std::wstring, Glow> glows_;
	UINT width_ = 0, height_ = 0;
	bool full_present_needed_ = true;
	bool drawing_ = false;
};

}  // namespace ng
