// The text box of the row of the moves on macOS: an NSTextField in the view (src/app_record.cpp has what is shared).
// Enter takes what was typed, Esc takes it back; a paste loads at once (the field editor of the window: app_mac.mm).
#import <Cocoa/Cocoa.h>

#include "../app.h"

#include <cmath>

#include "../lang.h"
#include "mac_record.h"
#include "mac_view.h"

namespace {

NSString *ns(const std::wstring &w) { return [NSString stringWithUTF8String:ng::narrow(w).c_str()] ?: @""; }

NSColor *nscolor(const ng::Color &c) { return [NSColor colorWithSRGBRed:c.r green:c.g blue:c.b alpha:c.a]; }

}  // namespace

@implementation NgRecordBox
- (BOOL)control:(NSControl *)control textView:(NSTextView *)textView doCommandBySelector:(SEL)sel
{
	if (sel == @selector(insertNewline:)) {
		self.app->record_key(VK_RETURN);
		return YES;
	}
	if (sel == @selector(cancelOperation:)) {
		self.app->record_key(VK_ESCAPE);
		return YES;
	}
	return NO;
}
- (void)controlTextDidBeginEditing:(NSNotification *)note
{
	self.app->record_resume_ = false;
}
- (void)controlTextDidChange:(NSNotification *)note
{
	self.modified = YES;
}
- (void)controlTextDidEndEditing:(NSNotification *)note
{
	// to the board of this window: taken back; to another window or program: it stays, and the keys come back to it
	NSWindow *w = self.field.window;
	const bool to_board = w.isKeyWindow && [w.firstResponder isKindOfClass:[NgView class]];
	self.app->record_focus_lost(to_board ? self.app->hwnd_ : nullptr);
}
- (void)paste
{
	self.app->record_take(ng::narrow(self.app->clipboard_text()));
}
@end

namespace ng {

namespace {

NgRecordBox *box_of(const App &a) { return a.record_edit_ ? (__bridge NgRecordBox *)a.record_edit_ : nil; }

bool editing(NgRecordBox *b)
{
	NSText *ed = b.field.currentEditor;
	return ed && b.field.window.firstResponder == ed;
}

}  // namespace

App::~App()
{
	if (record_edit_) {
		NgRecordBox *b = (__bridge_transfer NgRecordBox *)record_edit_;
		[b.field removeFromSuperview];
		record_edit_ = nullptr;
	}
}

void App::create_record_box()
{
	if (!hwnd_) return;
	NgView *view = (__bridge NgView *)hwnd_;
	NgRecordBox *b = [[NgRecordBox alloc] init];
	b.app = this;
	NSTextField *f = [NSTextField textFieldWithString:@""];
	f.bordered = NO;
	f.bezeled = NO;
	f.drawsBackground = YES;
	f.focusRingType = NSFocusRingTypeNone;
	f.usesSingleLineMode = YES;
	f.cell.scrollable = YES;
	f.cell.wraps = NO;
	f.delegate = b;
	f.hidden = YES;
	[view addSubview:f];
	b.field = f;
	record_edit_ = (__bridge_retained void *)b;
}

void App::place_record_box()
{
	NgRecordBox *b = box_of(*this);
	if (!b) return;
	const Layout &L = lay_;
	const RectF &T = L.record_text;
	if (T.right <= T.left) {
		// (the keys in another window meanwhile: what was typed is taken back when the box is shown again)
		leave_record_box();
		if (b.modified) sync_record_box(true);
		b.field.hidden = YES;
		return;
	}
	const int px = static_cast<int>(L.font_s);
	if (px != record_font_px_) {
		b.field.font = [NSFont systemFontOfSize:px];
		record_font_px_ = px;
	}
	const Theme &th = theme(design_);
	b.field.backgroundColor = nscolor(th.panel2);
	b.field.textColor = nscolor(th.text);
	b.field.placeholderString = ns(lang::t("record.hint"));
	const float pad = std::round(6 * L.scale);
	const float h = std::round(px * 1.4f);
	const float y = std::round((T.top + T.bottom - h) / 2);
	b.field.frame = NSMakeRect(T.left + pad, y, T.right - T.left - 2 * pad, h);
	b.field.hidden = NO;
}

void App::sync_record_box(bool force)
{
	NgRecordBox *b = box_of(*this);
	if (!b) return;
	if (editing_) leave_record_box();
	b.field.enabled = !editing_;
	if (!force && b.modified) return;
	std::wstring text = record_text();
	if (!force && text == record_shown_) return;
	b.field.stringValue = ns(text);
	b.modified = NO;
	record_shown_ = std::move(text);
	// the last moves are the ones in sight
	if (NSText *ed = b.field.currentEditor) {
		const NSRange end = NSMakeRange(ed.string.length, 0);
		ed.selectedRange = end;
		[ed scrollRangeToVisible:end];
	}
}

void App::leave_record_box()
{
	record_resume_ = false;
	NgRecordBox *b = box_of(*this);
	if (b && editing(b)) [b.field.window makeFirstResponder:(__bridge NgView *)hwnd_];
}

void App::record_focus_lost(HWND to)
{
	if (!to) {
		record_resume_ = true;
		return;
	}
	sync_record_box(true);
}

bool App::record_resumes()
{
	const bool r = record_resume_;
	record_resume_ = false;
	NgRecordBox *b = box_of(*this);
	return r && b && !b.field.hidden && b.field.enabled;
}

void App::record_key(WPARAM vk)
{
	NgRecordBox *b = box_of(*this);
	if (b && vk == VK_RETURN && b.modified) {
		record_take(narrow(std::wstring(widen(std::string_view(b.field.stringValue.UTF8String ?: "")))));
		return;
	}
	sync_record_box(true);
	leave_record_box();
}

LRESULT App::record_color(HDC) { return 0; }  // (the colors are set in place_record_box)

}  // namespace ng
