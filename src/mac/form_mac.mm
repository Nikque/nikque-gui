// run_form on macOS: an alert with the fields in a grid (a label, the control, the hint) and OK / Cancel. A number out
// of its range is said in a box and the window comes again with what was typed.
#import <Cocoa/Cocoa.h>

#include "../form.h"

#include <cwchar>

#include "../ini.h"
#include "../lang.h"
#include "mac_view.h"

namespace {

NSString *ns(const std::wstring &w) { return [NSString stringWithUTF8String:ng::narrow(w).c_str()] ?: @""; }

std::wstring from_ns(NSString *s) { return s ? ng::widen(std::string_view(s.UTF8String ?: "")) : std::wstring(); }

// the height of the alert besides its fields (its icon, title and buttons), with a margin
constexpr CGFloat kAlertRoom = 240;

}  // namespace

// a slider and the number beside it follow each other
@interface NgFormSync : NSObject <NSTextFieldDelegate>
@property(nonatomic, weak) NSSlider *slider;
@property(nonatomic, weak) NSTextField *number;
@property(nonatomic) long lo, hi;
@end

@implementation NgFormSync
- (void)sliderMoved:(id)sender
{
	self.number.stringValue = [NSString stringWithFormat:@"%ld", lround(self.slider.doubleValue)];
}
- (void)controlTextDidChange:(NSNotification *)note
{
	NSString *s = self.number.stringValue;
	NSScanner *sc = [NSScanner scannerWithString:s];
	long long v = 0;
	if ([sc scanLongLong:&v] && sc.isAtEnd && v >= self.lo && v <= self.hi) self.slider.doubleValue = static_cast<double>(v);
}
@end

namespace ng {

bool run_form(NativeWindow, const std::wstring &title, std::vector<FormField> &fields)
{
	const std::wstring &ok = lang::t("form.ok"), &cancel = lang::t("form.cancel"), &bad_number = lang::t("form.bad_number");
	for (;;) {
		NSAlert *a = [[NSAlert alloc] init];
		a.messageText = ns(title);
		[a addButtonWithTitle:ns(ok)];
		[a addButtonWithTitle:ns(cancel)];
		a.buttons[1].keyEquivalent = @"\033";

		NSMutableArray<NSArray<NSView *> *> *rows = [NSMutableArray array];
		NSMutableArray *syncs = [NSMutableArray array];  // (kept while the window is open)
		std::vector<NSView *> controls(fields.size(), nil);
		for (size_t i = 0; i < fields.size(); ++i) {
			FormField &f = fields[i];
			NSTextField *label = [NSTextField labelWithString:ns(f.label)];
			NSTextField *note = [NSTextField labelWithString:ns(f.note)];
			note.textColor = NSColor.secondaryLabelColor;
			NSView *control = nil;
			switch (f.kind) {
			case FormField::Heading:
				label.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
				[rows addObject:@[label, [NSGridCell emptyContentView], note]];
				continue;
			case FormField::Number:
			case FormField::Text: {
				NSTextField *t = [NSTextField textFieldWithString:ns(f.text)];
				[t.widthAnchor constraintGreaterThanOrEqualToConstant:f.kind == FormField::Text ? 260 : 80].active = YES;
				control = t;
				break;
			}
			case FormField::Slider: {
				NSTextField *t = [NSTextField textFieldWithString:ns(f.text)];
				[t.widthAnchor constraintEqualToConstant:60].active = YES;
				NSSlider *s = [NSSlider sliderWithValue:std::wcstol(f.text.c_str(), nullptr, 10) minValue:f.lo maxValue:f.hi
				                                 target:nil action:nil];
				[s.widthAnchor constraintGreaterThanOrEqualToConstant:160].active = YES;
				NgFormSync *sync = [[NgFormSync alloc] init];
				sync.slider = s;
				sync.number = t;
				sync.lo = f.lo;
				sync.hi = f.hi;
				s.target = sync;
				s.action = @selector(sliderMoved:);
				t.delegate = sync;
				[syncs addObject:sync];
				NSStackView *st = [NSStackView stackViewWithViews:@[t, s]];
				controls[i] = t;
				[rows addObject:@[label, st, note]];
				continue;
			}
			case FormField::Choice: {
				NSPopUpButton *p = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
				for (const auto &c : f.choices) [p addItemWithTitle:ns(c)];
				if (f.choice >= 0 && f.choice < static_cast<int>(f.choices.size())) [p selectItemAtIndex:f.choice];
				control = p;
				break;
			}
			case FormField::Check: {
				NSButton *b = [NSButton checkboxWithTitle:ns(f.label) target:nil action:nil];
				b.state = f.checked ? NSControlStateValueOn : NSControlStateValueOff;
				control = b;
				label = [NSTextField labelWithString:@""];
				break;
			}
			}
			controls[i] = control;
			[rows addObject:@[label, control, note]];
		}
		NSGridView *grid = [NSGridView gridViewWithViews:rows];
		grid.rowSpacing = 8;
		grid.columnSpacing = 10;
		[grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;
		grid.frame = NSMakeRect(0, 0, grid.fittingSize.width, grid.fittingSize.height);
		// fields higher than the screen scroll (from their top), with OK and Cancel under them
		NSView *fields_view = grid;
		NSScreen *screen = NSApp.mainWindow.screen ?: NSScreen.mainScreen;
		const CGFloat room = screen ? screen.visibleFrame.size.height - kAlertRoom : 0;
		if (room > 120 && grid.frame.size.height > room) {
			NSScrollView *sv = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, grid.frame.size.width + 20, room)];
			sv.hasVerticalScroller = YES;
			sv.drawsBackground = NO;
			sv.borderType = NSNoBorder;
			sv.documentView = grid;
			[sv.contentView scrollToPoint:NSMakePoint(0, grid.frame.size.height - room)];
			[sv reflectScrolledClipView:sv.contentView];
			fields_view = sv;
		}
		a.accessoryView = fields_view;

		ng_modal_begin();
		const NSModalResponse r = [a runModal];
		ng_modal_end();

		// what is in the controls now (kept when the window comes again)
		std::wstring bad;
		for (size_t i = 0; i < fields.size(); ++i) {
			FormField &f = fields[i];
			NSView *c = controls[i];
			if (!c) continue;
			switch (f.kind) {
			case FormField::Number:
			case FormField::Text:
			case FormField::Slider: {
				f.text = from_ns(static_cast<NSTextField *>(c).stringValue);
				if (f.kind == FormField::Text || !bad.empty()) break;
				wchar_t *end = nullptr;
				const long v = std::wcstol(f.text.c_str(), &end, 10);
				if (f.text.empty() || *end || v < f.lo || v > f.hi) {
					wchar_t msg[512];
					swprintf(msg, 512, L"%ls\n%ls: %d .. %d", bad_number.c_str(), f.label.c_str(), f.lo, f.hi);
					bad = msg;
					if (bad.empty()) bad = bad_number;
				}
				break;
			}
			case FormField::Choice: f.choice = static_cast<int>(static_cast<NSPopUpButton *>(c).indexOfSelectedItem); break;
			case FormField::Check: f.checked = static_cast<NSButton *>(c).state == NSControlStateValueOn; break;
			case FormField::Heading: break;
			}
		}
		if (r != NSAlertFirstButtonReturn) return false;
		if (bad.empty()) return true;
		message_box(nullptr, bad, title, MB_ICONWARNING);
	}
}

}  // namespace ng
