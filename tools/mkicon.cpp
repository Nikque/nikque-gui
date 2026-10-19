// Draws the icons at build time: mkicon <dir> writes <dir>\app.ico and <dir>\paw.ico (PNG images inside).
#include <d2d1.h>
#include <objbase.h>
#include <wincodec.h>
#include <windows.h>
#include <wrl/client.h>

#include <cstdio>
#include <string>
#include <vector>

#include "../src/icon_draw.h"

using Microsoft::WRL::ComPtr;

static std::vector<unsigned char> render_png(ID2D1Factory *f, IWICImagingFactory *wic, UINT size, bool paw)
{
	ComPtr<IWICBitmap> bmp;
	wic->CreateBitmap(size, size, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bmp);
	ComPtr<ID2D1RenderTarget> rt;
	const D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
		D2D1_RENDER_TARGET_TYPE_SOFTWARE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
		96, 96);
	f->CreateWicBitmapRenderTarget(bmp.Get(), props, &rt);
	rt->BeginDraw();
	rt->Clear(D2D1::ColorF(0, 0, 0, 0));
	ng::draw_icon(f, rt.Get(), static_cast<float>(size), paw);
	rt->EndDraw();

	ComPtr<IStream> stream;
	CreateStreamOnHGlobal(nullptr, TRUE, &stream);
	ComPtr<IWICBitmapEncoder> enc;
	wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
	enc->Initialize(stream.Get(), WICBitmapEncoderNoCache);
	ComPtr<IWICBitmapFrameEncode> frame;
	enc->CreateNewFrame(&frame, nullptr);
	frame->Initialize(nullptr);
	frame->SetSize(size, size);
	WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
	frame->SetPixelFormat(&fmt);
	ComPtr<IWICFormatConverter> conv;
	wic->CreateFormatConverter(&conv);
	conv->Initialize(bmp.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
	                 WICBitmapPaletteTypeCustom);
	frame->WriteSource(conv.Get(), nullptr);
	frame->Commit();
	enc->Commit();
	HGLOBAL h;
	GetHGlobalFromStream(stream.Get(), &h);
	const SIZE_T n = GlobalSize(h);
	std::vector<unsigned char> out(n);
	memcpy(out.data(), GlobalLock(h), n);
	GlobalUnlock(h);
	// the stream may be larger than the data written
	STATSTG st{};
	stream->Stat(&st, STATFLAG_NONAME);
	out.resize(static_cast<size_t>(st.cbSize.QuadPart));
	return out;
}

static bool write_ico(const std::wstring &path, ID2D1Factory *f, IWICImagingFactory *wic, bool paw)
{
	const UINT sizes[] = {16, 20, 24, 32, 40, 48, 64, 96, 128, 256};
	std::vector<std::vector<unsigned char>> pngs;
	for (UINT s : sizes) pngs.push_back(render_png(f, wic, s, paw));
	FILE *fp = nullptr;
	if (_wfopen_s(&fp, path.c_str(), L"wb") != 0 || !fp) return false;
	auto w16 = [&](unsigned v) { fputc(v & 0xff, fp); fputc((v >> 8) & 0xff, fp); };
	auto w32 = [&](unsigned v) { w16(v & 0xffff); w16(v >> 16); };
	const unsigned n = static_cast<unsigned>(std::size(sizes));
	w16(0);
	w16(1);
	w16(n);
	unsigned offset = 6 + 16 * n;
	for (unsigned i = 0; i < n; ++i) {
		fputc(sizes[i] >= 256 ? 0 : sizes[i], fp);
		fputc(sizes[i] >= 256 ? 0 : sizes[i], fp);
		fputc(0, fp);
		fputc(0, fp);
		w16(1);
		w16(32);
		w32(static_cast<unsigned>(pngs[i].size()));
		w32(offset);
		offset += static_cast<unsigned>(pngs[i].size());
	}
	for (auto &p : pngs) fwrite(p.data(), 1, p.size(), fp);
	return fclose(fp) == 0;
}

int wmain(int argc, wchar_t **argv)
{
	if (argc < 2) {
		std::fprintf(stderr, "usage: mkicon <dir>\n");
		return 2;
	}
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	bool ok;
	{  // the COM objects go before CoUninitialize
		ComPtr<ID2D1Factory> f;
		D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, f.GetAddressOf());
		ComPtr<IWICImagingFactory> wic;
		CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic));
		const std::wstring dir = argv[1];
		ok = write_ico(dir + L"\\app.ico", f.Get(), wic.Get(), false) &&
		     write_ico(dir + L"\\paw.ico", f.Get(), wic.Get(), true);
	}
	CoUninitialize();
	return ok ? 0 : 1;
}
