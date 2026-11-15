// mkicon_mac <folder.iconset>: the icon of the app for macOS, drawn at build time (no image files in the sources): the
// same picture as the icon of Windows (src/win/icon_draw.cpp: a blue board square with a black and a white disc), with
// the margin of the icons of macOS, in the sizes of an iconset (made into AppIcon.icns by iconutil).
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>

#include <cstdio>
#include <string>

namespace {

void set(CGContextRef g, unsigned v, bool fill)
{
	const CGFloat r = ((v >> 16) & 0xff) / 255.0, gr = ((v >> 8) & 0xff) / 255.0, b = (v & 0xff) / 255.0;
	if (fill) CGContextSetRGBFillColor(g, r, gr, b, 1);
	else CGContextSetRGBStrokeColor(g, r, gr, b, 1);
}

void disc(CGContextRef g, CGFloat x, CGFloat y, CGFloat d, unsigned fill, unsigned edge, CGFloat w)
{
	const CGRect rc = CGRectMake(x - d, y - d, 2 * d, 2 * d);
	set(g, fill, true);
	CGContextFillEllipseInRect(g, rc);
	set(g, edge, false);
	CGContextSetLineWidth(g, w);
	CGContextStrokeEllipseInRect(g, rc);
}

bool write_png(int px, const std::string &path)
{
	CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGContextRef g = CGBitmapContextCreate(nullptr, px, px, 8, 0, cs, kCGImageAlphaPremultipliedLast);
	CGColorSpaceRelease(cs);
	if (!g) return false;
	// the y axis down, as on Windows
	CGContextTranslateCTM(g, 0, px);
	CGContextScaleCTM(g, 1, -1);
	const CGFloat m = px * 0.1, s = px - 2 * m;  // (the margin of the icons of macOS)
	CGContextTranslateCTM(g, m, m);
	const CGFloat r = s * 0.2;
	CGPathRef sq = CGPathCreateWithRoundedRect(CGRectMake(0, 0, s, s), r, r, nullptr);
	set(g, 0x0684c7, true);
	CGContextAddPath(g, sq);
	CGContextFillPath(g);
	CGPathRelease(sq);
	const CGFloat d = s * 0.27, w = s / 48.0 + 0.5;
	disc(g, s * 0.63, s * 0.63, d, 0xf1f1f1, 0x9f9fa9, w);
	disc(g, s * 0.37, s * 0.37, d, 0x101113, 0x52525c, w);
	CGImageRef img = CGBitmapContextCreateImage(g);
	CGContextRelease(g);
	NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
	CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, nullptr);
	bool ok = false;
	if (dest && img) {
		CGImageDestinationAddImage(dest, img, nullptr);
		ok = CGImageDestinationFinalize(dest);
	}
	if (dest) CFRelease(dest);
	if (img) CGImageRelease(img);
	return ok;
}

}  // namespace

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::fprintf(stderr, "usage: mkicon_mac <folder.iconset>\n");
		return 2;
	}
	const std::string dir = argv[1];
	[[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithUTF8String:dir.c_str()]
	                          withIntermediateDirectories:YES
	                                           attributes:nil
	                                                error:nil];
	for (const int size : {16, 32, 128, 256, 512}) {
		const std::string base = dir + "/icon_" + std::to_string(size) + "x" + std::to_string(size);
		if (!write_png(size, base + ".png") || !write_png(size * 2, base + "@2x.png")) return 1;
	}
	return 0;
}
