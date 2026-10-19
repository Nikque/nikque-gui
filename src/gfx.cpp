#include "gfx.h"

#include <d2d1_1helper.h>
#include <d2d1effects.h>

#include <cmath>
#include <numbers>

namespace ng {

bool Gfx::init(HWND hwnd)
{
	hwnd_ = hwnd;
	D2D1_FACTORY_OPTIONS fo{};
	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &fo,
	                             reinterpret_cast<void **>(factory_.GetAddressOf()))))
		return false;
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
	                               reinterpret_cast<IUnknown **>(dwrite_.GetAddressOf()))))
		return false;

	D2D1_STROKE_STYLE_PROPERTIES sp = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND,
	                                                              D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
	factory_->CreateStrokeStyle(sp, nullptr, 0, &round_join_);

	// unit shapes
	auto make = [&](ComPtr<ID2D1PathGeometry> &g, const D2D1_POINT_2F *pts, int n) {
		factory_->CreatePathGeometry(&g);
		ComPtr<ID2D1GeometrySink> s;
		g->Open(&s);
		s->BeginFigure(pts[0], D2D1_FIGURE_BEGIN_FILLED);
		for (int i = 1; i < n; ++i) s->AddLine(pts[i]);
		s->EndFigure(D2D1_FIGURE_END_CLOSED);
		s->Close();
	};
	const D2D1_POINT_2F spark[] = {{0, -0.5f}, {0.09f, -0.09f}, {0.5f, 0}, {0.09f, 0.09f},
	                               {0, 0.5f},  {-0.09f, 0.09f}, {-0.5f, 0}, {-0.09f, -0.09f}};
	make(sparkle_, spark, 8);
	D2D1_POINT_2F st[10];
	for (int i = 0; i < 10; ++i) {
		const float a = static_cast<float>(-std::numbers::pi / 2 + i * std::numbers::pi / 5);
		const float r = (i & 1) ? 0.2f : 0.5f;
		st[i] = {r * std::cos(a), r * std::sin(a) + 0.04f};
	}
	make(star_, st, 10);
	{
		factory_->CreatePathGeometry(&heart_);
		ComPtr<ID2D1GeometrySink> s;
		heart_->Open(&s);
		s->BeginFigure({0, 0.42f}, D2D1_FIGURE_BEGIN_FILLED);
		s->AddBezier({{-0.62f, 0.02f}, {-0.36f, -0.52f}, {0, -0.2f}});
		s->AddBezier({{0.36f, -0.52f}, {0.62f, 0.02f}, {0, 0.42f}});
		s->EndFigure(D2D1_FIGURE_END_CLOSED);
		s->Close();
	}
	{
		// a paw: one large pad and four toes
		const D2D1_ELLIPSE parts[] = {
			{{0, 0.17f}, 0.27f, 0.22f},    {{-0.33f, -0.1f}, 0.1f, 0.125f}, {{-0.12f, -0.3f}, 0.105f, 0.13f},
			{{0.12f, -0.3f}, 0.105f, 0.13f}, {{0.33f, -0.1f}, 0.1f, 0.125f},
		};
		ID2D1Geometry *gs[5];
		ComPtr<ID2D1EllipseGeometry> es[5];
		for (int i = 0; i < 5; ++i) {
			factory_->CreateEllipseGeometry(parts[i], &es[i]);
			gs[i] = es[i].Get();
		}
		ComPtr<ID2D1GeometryGroup> grp;
		factory_->CreateGeometryGroup(D2D1_FILL_MODE_WINDING, gs, 5, &grp);
		paw_ = grp;
	}
	{
		factory_->CreatePathGeometry(&rays_);
		ComPtr<ID2D1GeometrySink> s;
		rays_->Open(&s);
		for (int i = 0; i < 10; ++i) {
			const double a = i * std::numbers::pi / 5, w = 0.07;
			s->BeginFigure({0, 0}, D2D1_FIGURE_BEGIN_FILLED);
			s->AddLine({static_cast<float>(std::cos(a - w)), static_cast<float>(std::sin(a - w))});
			s->AddLine({static_cast<float>(std::cos(a + w)), static_cast<float>(std::sin(a + w))});
			s->EndFigure(D2D1_FIGURE_END_CLOSED);
		}
		s->Close();
	}
	return create_device();
}

bool Gfx::create_device()
{
	const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
	                                    D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3};
	const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 5, D3D11_SDK_VERSION,
	                               &d3d_, nullptr, nullptr);
	if (FAILED(hr))
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels, 5, D3D11_SDK_VERSION, &d3d_,
		                       nullptr, nullptr);
	if (FAILED(hr)) return false;
	ComPtr<IDXGIDevice1> dxgi;
	d3d_.As(&dxgi);
	dxgi->SetMaximumFrameLatency(1);
	if (FAILED(factory_->CreateDevice(dxgi.Get(), &device_))) return false;
	if (FAILED(device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc_))) return false;
	dc_.As(&dc1_);
	dc_->SetDpi(96.0f, 96.0f);
	dc_->SetUnitMode(D2D1_UNIT_MODE_PIXELS);

	ComPtr<IDXGIAdapter> adapter;
	dxgi->GetAdapter(&adapter);
	ComPtr<IDXGIFactory2> dxgi_factory;
	adapter->GetParent(IID_PPV_ARGS(&dxgi_factory));
	DXGI_SWAP_CHAIN_DESC1 sd{};
	sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.SampleDesc.Count = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2;
	sd.Scaling = DXGI_SCALING_NONE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	if (FAILED(dxgi_factory->CreateSwapChainForHwnd(d3d_.Get(), hwnd_, &sd, nullptr, nullptr, &swap_))) return false;
	dxgi_factory->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
	dc_->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0), &brush_);
	create_target();
	return true;
}

void Gfx::create_target()
{
	ComPtr<IDXGISurface> surface;
	if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(&surface)))) return;
	const D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE), 96.0f, 96.0f);
	dc_->CreateBitmapFromDxgiSurface(surface.Get(), &bp, &target_);
	dc_->SetTarget(target_.Get());
	const D2D1_SIZE_U s = target_->GetPixelSize();
	width_ = s.width;
	height_ = s.height;
	full_present_needed_ = true;
}

void Gfx::release_device()
{
	glows_.clear();
	gradients_.clear();
	shapes_.clear();  // the realizations belong to the device
	brush_.Reset();
	target_.Reset();
	dc_.Reset();
	dc1_.Reset();
	device_.Reset();
	swap_.Reset();
	d3d_.Reset();
}

void Gfx::resize()
{
	if (!swap_) return;
	RECT rc;
	GetClientRect(hwnd_, &rc);
	const UINT w = std::max<LONG>(1, rc.right - rc.left), h = std::max<LONG>(1, rc.bottom - rc.top);
	if (w == width_ && h == height_) return;
	dc_->SetTarget(nullptr);
	target_.Reset();
	if (FAILED(swap_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN, 0))) {
		release_device();
		create_device();
		return;
	}
	create_target();
}

bool Gfx::begin_frame()
{
	if (!dc_ && !create_device()) return false;
	if (!target_) return false;
	dc_->BeginDraw();
	dc_->SetTransform(D2D1::Matrix3x2F::Identity());
	drawing_ = true;
	return true;
}

void Gfx::end_frame(const std::vector<D2D1_RECT_F> *dirty)
{
	drawing_ = false;
	const HRESULT hr = dc_->EndDraw();
	if (hr == D2DERR_RECREATE_TARGET) {
		release_device();
		return;
	}
	HRESULT pr;
	if (dirty && !dirty->empty() && !full_present_needed_) {
		std::vector<RECT> rects;
		rects.reserve(dirty->size());
		for (const auto &r : *dirty) {
			RECT x{static_cast<LONG>(std::floor(r.left)), static_cast<LONG>(std::floor(r.top)),
			       static_cast<LONG>(std::ceil(r.right)), static_cast<LONG>(std::ceil(r.bottom))};
			x.left = std::max<LONG>(0, x.left);
			x.top = std::max<LONG>(0, x.top);
			x.right = std::min<LONG>(width_, x.right);
			x.bottom = std::min<LONG>(height_, x.bottom);
			if (x.right > x.left && x.bottom > x.top) rects.push_back(x);
		}
		DXGI_PRESENT_PARAMETERS pp{static_cast<UINT>(rects.size()), rects.data(), nullptr, nullptr};
		pr = swap_->Present1(1, 0, &pp);
	} else {
		DXGI_PRESENT_PARAMETERS pp{};
		pr = swap_->Present1(1, 0, &pp);
		full_present_needed_ = false;
	}
	if (pr == DXGI_ERROR_DEVICE_REMOVED || pr == DXGI_ERROR_DEVICE_RESET) release_device();
}

ID2D1SolidColorBrush *Gfx::brush(const D2D1_COLOR_F &c)
{
	brush_->SetColor(c);
	brush_->SetOpacity(1.0f);
	return brush_.Get();
}

ID2D1LinearGradientBrush *Gfx::gradient(int id, const D2D1_GRADIENT_STOP *stops, UINT n)
{
	auto it = gradients_.find(id);
	if (it != gradients_.end()) return it->second.Get();
	ComPtr<ID2D1GradientStopCollection> sc;
	dc_->CreateGradientStopCollection(stops, n, &sc);
	ComPtr<ID2D1LinearGradientBrush> b;
	dc_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0, 0}, {0, 1}), sc.Get(), &b);
	return (gradients_[id] = b).Get();
}

IDWriteTextFormat *Gfx::text(float px, bool bold, DWRITE_TEXT_ALIGNMENT align, Face face)
{
	const std::wstring key = std::to_wstring(static_cast<int>(px * 4)) + (bold ? L"b" : L"n") +
	                         std::to_wstring(static_cast<int>(align)) + (face == Face::Number ? L"N" : L"U");
	auto it = formats_.find(key);
	if (it != formats_.end()) return it->second.Get();
	ComPtr<IDWriteTextFormat> f;
	dwrite_->CreateTextFormat(face == Face::Number ? L"Segoe UI" : L"Yu Gothic UI", nullptr,
	                          bold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
	                          DWRITE_FONT_STRETCH_NORMAL, px, L"ja-jp", &f);
	f->SetTextAlignment(align);
	f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
	return (formats_[key] = f).Get();
}

float Gfx::text_width(std::wstring_view s, IDWriteTextFormat *f)
{
	ComPtr<IDWriteTextLayout> l;
	if (FAILED(dwrite_->CreateTextLayout(s.data(), static_cast<UINT32>(s.size()), f, 10000.0f, 1000.0f, &l)))
		return 0;
	DWRITE_TEXT_METRICS m{};
	l->GetMetrics(&m);
	return m.widthIncludingTrailingWhitespace;
}

void Gfx::draw_text(std::wstring_view s, IDWriteTextFormat *f, const D2D1_RECT_F &r, const D2D1_COLOR_F &c, bool clip)
{
	dc_->DrawText(s.data(), static_cast<UINT32>(s.size()), f, r, brush(c),
	              clip ? D2D1_DRAW_TEXT_OPTIONS_CLIP : D2D1_DRAW_TEXT_OPTIONS_NONE);
}

ComPtr<IDWriteFontFace> Gfx::font_face(Face face)
{
	ComPtr<IDWriteFontFace> &slot = face == Face::Number ? number_face_ : ui_face_;
	if (slot) return slot;
	ComPtr<IDWriteFontCollection> fc;
	dwrite_->GetSystemFontCollection(&fc);
	UINT32 index = 0;
	BOOL exists = FALSE;
	fc->FindFamilyName(face == Face::Number ? L"Segoe UI" : L"Yu Gothic UI", &index, &exists);
	if (!exists) fc->FindFamilyName(L"Segoe UI", &index, &exists);
	if (!exists) index = 0;
	ComPtr<IDWriteFontFamily> fam;
	fc->GetFontFamily(index, &fam);
	ComPtr<IDWriteFont> font;
	fam->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, &font);
	font->CreateFontFace(&slot);
	return slot;
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

	ComPtr<IDWriteFontFace> ff = font_face(face);
	std::vector<UINT32> cps;
	for (size_t i = 0; i < text.size(); ++i) {
		UINT32 c = text[i];
		if (c >= 0xd800 && c < 0xdc00 && i + 1 < text.size()) c = 0x10000 + ((c - 0xd800) << 10) + (text[++i] - 0xdc00);
		cps.push_back(c);
	}
	std::vector<UINT16> glyphs(cps.size());
	ff->GetGlyphIndices(cps.data(), static_cast<UINT32>(cps.size()), glyphs.data());
	if (face == Face::Number) {
		for (UINT16 g : glyphs)
			if (g == 0) {  // not in the number font: the UI font
				ff = font_face(Face::Ui);
				ff->GetGlyphIndices(cps.data(), static_cast<UINT32>(cps.size()), glyphs.data());
				break;
			}
	}
	DWRITE_FONT_METRICS fm;
	ff->GetMetrics(&fm);
	std::vector<DWRITE_GLYPH_METRICS> gm(glyphs.size());
	ff->GetDesignGlyphMetrics(glyphs.data(), static_cast<UINT32>(glyphs.size()), gm.data());
	std::vector<float> adv(glyphs.size());
	for (size_t i = 0; i < glyphs.size(); ++i) adv[i] = gm[i].advanceWidth * em / fm.designUnitsPerEm;

	NumberShape s;
	factory_->CreatePathGeometry(&s.geom);
	ComPtr<ID2D1GeometrySink> sink;
	s.geom->Open(&sink);
	sink->SetFillMode(D2D1_FILL_MODE_WINDING);
	ff->GetGlyphRunOutline(em, glyphs.data(), adv.data(), nullptr, static_cast<UINT32>(glyphs.size()), FALSE, FALSE,
	                       sink.Get());
	sink->Close();
	s.geom->GetBounds(nullptr, &s.bounds);
	if (shapes_.size() > 2048) shapes_.clear();
	return shapes_[key] = std::move(s);
}

static std::wstring glow_key(std::wstring_view shape_key, const D2D1_COLOR_F &c, float radius)
{
	wchar_t b[96];
	swprintf(b, 96, L"|%02x%02x%02x%02x|%d", static_cast<int>(c.r * 255), static_cast<int>(c.g * 255),
	         static_cast<int>(c.b * 255), static_cast<int>(c.a * 255), static_cast<int>(radius * 4));
	return std::wstring(shape_key) + b;
}

void Gfx::prepare_glow(std::wstring_view text, float em, const D2D1_COLOR_F &color, float radius, Face face)
{
	if (!dc_ || drawing_) return;
	const std::wstring key = glow_key(shape_key(text, em, face), color, radius);
	if (glows_.count(key)) return;
	const NumberShape &s = shape(text, em, face);
	const float pad = std::ceil(radius * 3.0f) + 2.0f;
	const D2D1_SIZE_U size{static_cast<UINT32>(std::ceil(s.bounds.right - s.bounds.left + 2 * pad)),
	                       static_cast<UINT32>(std::ceil(s.bounds.bottom - s.bounds.top + 2 * pad))};
	if (size.width == 0 || size.height == 0 || size.width > 4096 || size.height > 4096) return;
	const D2D1_BITMAP_PROPERTIES1 bp = D2D1::BitmapProperties1(
		D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
	ComPtr<ID2D1Bitmap1> mask, out;
	if (FAILED(dc_->CreateBitmap(size, nullptr, 0, bp, &mask)) || FAILED(dc_->CreateBitmap(size, nullptr, 0, bp, &out)))
		return;
	ComPtr<ID2D1Image> old;
	dc_->GetTarget(&old);
	const D2D1_POINT_2F origin{pad - s.bounds.left, pad - s.bounds.top};

	dc_->SetTarget(mask.Get());
	dc_->BeginDraw();
	dc_->Clear(D2D1::ColorF(0, 0, 0, 0));
	dc_->SetTransform(D2D1::Matrix3x2F::Translation(origin.x, origin.y));
	dc_->FillGeometry(s.geom.Get(), brush(D2D1::ColorF(1, 1, 1, 1)));
	dc_->EndDraw();

	ComPtr<ID2D1Effect> shadow;
	dc_->CreateEffect(CLSID_D2D1Shadow, &shadow);
	shadow->SetInput(0, mask.Get());
	shadow->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, radius);
	shadow->SetValue(D2D1_SHADOW_PROP_COLOR, D2D1::Vector4F(color.r, color.g, color.b, color.a));
	dc_->SetTarget(out.Get());
	dc_->BeginDraw();
	dc_->SetTransform(D2D1::Matrix3x2F::Identity());
	dc_->Clear(D2D1::ColorF(0, 0, 0, 0));
	dc_->DrawImage(shadow.Get());
	dc_->EndDraw();

	dc_->SetTarget(old.Get());
	if (glows_.size() > 512) glows_.clear();
	glows_[key] = Glow{out, {-origin.x, -origin.y}};
}

const Glow *Gfx::glow(std::wstring_view text, float em, const D2D1_COLOR_F &color, float radius, Face face) const
{
	auto it = glows_.find(glow_key(shape_key(text, em, face), color, radius));
	return it == glows_.end() ? nullptr : &it->second;
}

void Gfx::clear_shapes()
{
	shapes_.clear();
	glows_.clear();
}

}  // namespace ng

namespace ng {

// the realizations are made for scales up to 1.1 (the pulse of the stages 6 and 7)
static float tolerance()
{
	return D2D1::ComputeFlatteningTolerance(D2D1::Matrix3x2F::Identity(), 96.0f, 96.0f, 1.1f);
}

void Gfx::fill_shape(const NumberShape &s, ID2D1Brush *b)
{
	if (dc1_ && !s.fill_real) dc1_->CreateFilledGeometryRealization(s.geom.Get(), tolerance(), &s.fill_real);
	if (s.fill_real) dc1_->DrawGeometryRealization(s.fill_real.Get(), b);
	else dc_->FillGeometry(s.geom.Get(), b);
}

void Gfx::stroke_shape(const NumberShape &s, ID2D1Brush *b, float stroke_w)
{
	if (dc1_ && (!s.stroke_real || s.stroke_w != stroke_w)) {
		s.stroke_real.Reset();
		dc1_->CreateStrokedGeometryRealization(s.geom.Get(), tolerance(), stroke_w, round_join_.Get(), &s.stroke_real);
		s.stroke_w = stroke_w;
	}
	if (s.stroke_real) dc1_->DrawGeometryRealization(s.stroke_real.Get(), b);
	else dc_->DrawGeometry(s.geom.Get(), b, stroke_w, round_join_.Get());
}

}  // namespace ng
