#include "icon_draw.h"

#include <d2d1helper.h>
#include <wrl/client.h>

namespace ng {

using Microsoft::WRL::ComPtr;

namespace {

D2D1_COLOR_F hex(unsigned v, float a = 1) { return D2D1::ColorF(v, a); }

}  // namespace

void draw_icon(ID2D1Factory *factory, ID2D1RenderTarget *rt, float s, bool paw)
{
	(void)factory;
	ComPtr<ID2D1SolidColorBrush> b;
	rt->CreateSolidColorBrush(hex(0x000000), &b);
	const float r = s * 0.2f;
	if (!paw) {
		// a blue board square with a black and a white disc
		b->SetColor(hex(0x0684c7));
		rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, s, s), r, r), b.Get());
		const float d = s * 0.27f;
		b->SetColor(hex(0xf1f1f1));
		rt->FillEllipse(D2D1::Ellipse({s * 0.63f, s * 0.63f}, d, d), b.Get());
		b->SetColor(hex(0x9f9fa9));
		rt->DrawEllipse(D2D1::Ellipse({s * 0.63f, s * 0.63f}, d, d), b.Get(), s / 48.0f + 0.5f);
		b->SetColor(hex(0x101113));
		rt->FillEllipse(D2D1::Ellipse({s * 0.37f, s * 0.37f}, d, d), b.Get());
		b->SetColor(hex(0x52525c));
		rt->DrawEllipse(D2D1::Ellipse({s * 0.37f, s * 0.37f}, d, d), b.Get(), s / 48.0f + 0.5f);
		return;
	}
	// a pink paw on milk yellow
	b->SetColor(hex(0xfdf3d9));
	rt->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(0, 0, s, s), r, r), b.Get());
	b->SetColor(hex(0xfccee8));
	rt->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(s * 0.03f, s * 0.03f, s * 0.97f, s * 0.97f), r, r), b.Get(),
	                         s * 0.06f);
	b->SetColor(hex(0xfb64b6));
	rt->FillEllipse(D2D1::Ellipse({s * 0.5f, s * 0.64f}, s * 0.23f, s * 0.19f), b.Get());
	const float toes[4][2] = {{0.22f, 0.42f}, {0.39f, 0.25f}, {0.61f, 0.25f}, {0.78f, 0.42f}};
	for (const auto &t : toes) rt->FillEllipse(D2D1::Ellipse({s * t[0], s * t[1]}, s * 0.09f, s * 0.105f), b.Get());
}

}  // namespace ng
