// The parts of App that are macOS' own: the window and its view, the events (as WndProc of src/win/app_win.cpp hands
// them to the App), the timers, the menu bar, the file dialogs.
#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "../app.h"

#include <cmath>
#include <filesystem>
#include <map>

#include "../lang.h"
#include "mac_record.h"
#include "mac_view.h"

namespace {

NSString *ns(const std::wstring &w) { return [NSString stringWithUTF8String:ng::narrow(w).c_str()] ?: @""; }

std::wstring from_ns(NSString *s) { return s ? ng::widen(std::string_view(s.UTF8String ?: "")) : std::wstring(); }

// the virtual key of Windows for a key of the Mac (0: none)
int virtual_key(NSEvent *e)
{
	switch (e.keyCode) {
	case kVK_LeftArrow: return VK_LEFT;
	case kVK_RightArrow: return VK_RIGHT;
	case kVK_UpArrow: return VK_UP;
	case kVK_DownArrow: return VK_DOWN;
	case kVK_Home: return VK_HOME;
	case kVK_End: return VK_END;
	case kVK_PageUp: return VK_PRIOR;
	case kVK_PageDown: return VK_NEXT;
	case kVK_Return:
	case kVK_ANSI_KeypadEnter: return VK_RETURN;
	case kVK_Escape: return VK_ESCAPE;
	case kVK_Delete: return VK_BACK;
	case kVK_ForwardDelete: return VK_DELETE;
	case kVK_Tab: return VK_TAB;
	case kVK_Space: return VK_SPACE;
	case kVK_Help: return VK_INSERT;
	case kVK_F1: return VK_F1;
	case kVK_F2: return VK_F2;
	case kVK_F3: return VK_F3;
	case kVK_F4: return VK_F4;
	case kVK_F5: return VK_F5;
	case kVK_F6: return VK_F6;
	case kVK_F7: return VK_F7;
	case kVK_F8: return VK_F8;
	case kVK_F9: return VK_F9;
	case kVK_F10: return VK_F10;
	case kVK_F11: return VK_F11;
	case kVK_F12: return VK_F12;
	default: break;
	}
	NSString *c = e.charactersIgnoringModifiers.uppercaseString;
	if (c.length == 1) {
		const unichar u = [c characterAtIndex:0];
		if ((u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9')) return u;
	}
	return 0;
}

}  // namespace

// ---- the window

@interface NgWindowDelegate : NSObject <NSWindowDelegate>
@property(nonatomic, assign) ng::App *app;
@property(nonatomic, strong) NSTextView *recordEditor;  // the field editor of the text box of the moves
@end

// the field editor of the text box of the moves: a paste loads the moves at once
@interface NgRecordEditor : NSTextView
@property(nonatomic, weak) NgRecordBox *box;
@end

@implementation NgRecordEditor
- (void)paste:(id)sender
{
	if (self.box) [self.box paste];
	else [super paste:sender];
}
@end

@interface NgView ()
@property(nonatomic, assign) ng::App *app;
@property(nonatomic) double wheel;  // the scrolling not yet given (a step is a notch of a wheel)
@property(nonatomic) int popupChoice;
@property(nonatomic) BOOL popupOpen;
@end

@implementation NgView {
	std::map<std::uintptr_t, NSTimer *> timers_;
	NSTrackingArea *tracking_;
}

- (instancetype)initWithApp:(ng::App *)app frame:(NSRect)frame
{
	self = [super initWithFrame:frame];
	if (self) {
		_app = app;
		self.needsPaint = YES;
		[self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
	}
	return self;
}

- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)e { return YES; }

- (void)updateTrackingAreas
{
	if (tracking_) [self removeTrackingArea:tracking_];
	tracking_ = [[NSTrackingArea alloc] initWithRect:NSZeroRect
	                                         options:NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited |
	                                                 NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect
	                                           owner:self
	                                        userInfo:nil];
	[self addTrackingArea:tracking_];
	[super updateTrackingAreas];
}

- (void)drawRect:(NSRect)dirty
{
	ng::App *a = self.app;
	if (!a) return;
	if (self.needsPaint) {
		self.needsPaint = NO;
		a->gfx_.set_in_view_draw(true);
		a->paint();
		a->gfx_.set_in_view_draw(false);
	}
	CGImageRef img = static_cast<CGImageRef>(a->gfx_.copy_frame_image());
	if (!img) return;
	CGContextRef g = NSGraphicsContext.currentContext.CGContext;
	const NSRect b = self.bounds;
	CGContextSaveGState(g);
	// (the view is flipped: the picture is turned to stand upright)
	CGContextTranslateCTM(g, 0, b.size.height);
	CGContextScaleCTM(g, 1, -1);
	CGContextSetInterpolationQuality(g, kCGInterpolationNone);
	const CGFloat w = static_cast<CGFloat>(a->gfx_.width()), h = static_cast<CGFloat>(a->gfx_.height());
	CGContextDrawImage(g, CGRectMake(0, b.size.height - h, w, h), img);
	CGContextRestoreGState(g);
	CGImageRelease(img);
}

- (void)setFrameSize:(NSSize)size
{
	[super setFrameSize:size];
	if (!self.app || !self.window) return;
	self.app->gfx_.resize();
	self.app->layout();
	self.app->update_animation_timer();
	self.app->invalidate();
}

- (void)viewDidChangeBackingProperties
{
	[super viewDidChangeBackingProperties];
	if (!self.app) return;
	self.app->gfx_.resize();
	self.app->gfx_.clear_shapes();
	self.app->layout();
	self.app->invalidate();
}

// ---- the mouse

- (NSPoint)at:(NSEvent *)e { return [self convertPoint:e.locationInWindow fromView:nil]; }

- (void)mouseDown:(NSEvent *)e
{
	const NSPoint p = [self at:e];
	[self.window makeFirstResponder:self];
	self.app->on_mouse_down(static_cast<int>(p.x), static_cast<int>(p.y), false, e.clickCount == 2);
}

- (void)rightMouseDown:(NSEvent *)e
{
	const NSPoint p = [self at:e];
	self.app->on_mouse_down(static_cast<int>(p.x), static_cast<int>(p.y), true, false);
}

- (void)mouseUp:(NSEvent *)e
{
	const NSPoint p = [self at:e];
	self.app->on_mouse_up(static_cast<int>(p.x), static_cast<int>(p.y));
}

- (void)mouseMoved:(NSEvent *)e
{
	const NSPoint p = [self at:e];
	self.app->on_mouse_move(static_cast<int>(p.x), static_cast<int>(p.y));
	[self updateCursorAt:p];
}

- (void)mouseDragged:(NSEvent *)e { [self mouseMoved:e]; }

- (void)mouseExited:(NSEvent *)e
{
	self.app->on_mouse_move(-1, -1);
	[NSCursor.arrowCursor set];
}

- (void)updateCursorAt:(NSPoint)pt
{
	ng::App &a = *self.app;
	const POINT p{static_cast<LONG>(pt.x), static_cast<LONG>(pt.y)};
	NSCursor *c = NSCursor.arrowCursor;
	if (a.drag_ == ng::App::Drag::Panel || (!a.panel_hidden_ && !a.lay_.panel_below && ng::inside(a.lay_.panel_split, p)))
		c = NSCursor.resizeLeftRightCursor;
	else if (a.drag_ == ng::App::Drag::Graph || (!a.panel_hidden_ && ng::inside(a.lay_.splitter, p)))
		c = NSCursor.resizeUpDownCursor;
	else if (!a.panel_hidden_ && (ng::inside(a.lay_.fx.box, p) || ng::inside(a.lay_.fy.box, p) || ng::inside(a.lay_.record_text, p)))
		c = NSCursor.IBeamCursor;
	[c set];
}

- (void)scrollWheel:(NSEvent *)e
{
	// a notch of a wheel is a step; the fine scrolling of a trackpad is gathered into steps
	self.wheel += e.hasPreciseScrollingDeltas ? e.scrollingDeltaY / 12.0 : e.scrollingDeltaY;
	const NSPoint p = [self at:e];
	while (std::fabs(self.wheel) >= 1) {
		const int step = self.wheel > 0 ? 1 : -1;
		self.wheel -= step;
		self.app->on_wheel(static_cast<int>(p.x), static_cast<int>(p.y), step * 120);
	}
	if (e.phase == NSEventPhaseEnded || e.momentumPhase == NSEventPhaseEnded) self.wheel = 0;
}

// ---- the keys

- (void)keyDown:(NSEvent *)e
{
	if (const int vk = virtual_key(e)) self.app->on_key(static_cast<WPARAM>(vk));
	// the letters typed (as WM_CHAR), not with Command or Control
	if (e.modifierFlags & (NSEventModifierFlagCommand | NSEventModifierFlagControl)) return;
	NSString *s = e.characters;
	for (NSUInteger i = 0; i < s.length; ++i) {
		unichar c = [s characterAtIndex:i];
		if (c >= 0xf700 && c <= 0xf8ff) continue;  // (the arrows and the other keys of functions)
		if (c == 0x7f) c = 0x08;                   // (the key that deletes back)
		self.app->on_char(static_cast<wchar_t>(c));
	}
}

// ---- files dropped on the window: a book

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info { return NSDragOperationCopy; }

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info
{
	NSArray<NSURL *> *urls = [info.draggingPasteboard readObjectsForClasses:@[[NSURL class]]
	                                                                 options:@{NSPasteboardURLReadingFileURLsOnlyKey: @YES}];
	if (urls.count == 0) return NO;
	self.app->switch_book(from_ns(urls.firstObject.path));
	return YES;
}

// ---- messages, timers, menus

- (void)handleMessage:(unsigned)msg wParam:(std::uintptr_t)wp lParam:(std::intptr_t)lp
{
	ng::App &a = *self.app;
	switch (msg) {
	case ng::WM_TASK: a.task_ended(); break;
	case ng::WM_SCRIPT: a.pump_script(); break;
	case ng::WM_REDUCE: a.reduce_counted(); break;
	case ng::WM_BOOK_PEEK: a.book_peek_answers(); break;
	case ng::WM_EVAL_WARN: a.eval_mismatch_box(); break;
	case ng::WM_ENGINE: a.on_engine(wp, lp != 0); break;
	case WM_CLOSE: a.close_window(); break;
	default: break;
	}
}

- (void)timerFired:(NSTimer *)t
{
	const std::uintptr_t id = [t.userInfo unsignedLongValue];
	ng::App &a = *self.app;
	if (id == ng::kTimerAnim) a.paint_animation();
	else if (id == ng::kTimerWatch) a.watch_engines();
	else if (id == ng::kTimerScript) a.pump_script();
	else if (id == ng::kTimerSave) a.watch_save();
	else ng::play_timer(a, id);
}

- (void)setTimer:(std::uintptr_t)tid ms:(unsigned)ms
{
	[self killTimer:tid];
	NSTimer *t = [NSTimer timerWithTimeInterval:ms / 1000.0 target:self selector:@selector(timerFired:) userInfo:@(tid) repeats:YES];
	[NSRunLoop.currentRunLoop addTimer:t forMode:NSRunLoopCommonModes];
	timers_[tid] = t;
}

- (void)killTimer:(std::uintptr_t)tid
{
	auto it = timers_.find(tid);
	if (it == timers_.end()) return;
	[it->second invalidate];
	timers_.erase(it);
}

- (void)menuCommand:(id)sender
{
	const int id = static_cast<int>([sender tag]);
	if (self.popupOpen) {
		self.popupChoice = id;
		return;
	}
	self.app->on_command(id);
}

- (int)popUpMenu:(NSMenu *)menu atX:(int)x y:(int)y
{
	for (NSMenuItem *i in menu.itemArray) i.target = self;
	self.popupOpen = YES;
	self.popupChoice = 0;
	[menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(x, y) inView:self];
	self.popupOpen = NO;
	return self.popupChoice;
}

- (void)setMenuBar:(NSMenu *)bar
{
	_menuBar = bar;
	// the menu of the app first (the name of the app, hiding, quitting), then the menus of the window
	NSMenu *main = [[NSMenu alloc] initWithTitle:@""];
	NSMenuItem *appItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
	NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"nikque GUI"];
	[appMenu addItemWithTitle:@"Hide nikque GUI" action:@selector(hide:) keyEquivalent:@"h"];
	[appMenu addItem:[NSMenuItem separatorItem]];
	[appMenu addItemWithTitle:@"Quit nikque GUI" action:@selector(terminate:) keyEquivalent:@"q"];
	appItem.submenu = appMenu;
	[main addItem:appItem];
	for (NSMenuItem *i in [bar.itemArray copy]) {
		[bar removeItem:i];
		[main addItem:i];
	}
	_menuBar = main;
	[self setTargets:main];
	NSApp.mainMenu = main;
}

- (void)setTargets:(NSMenu *)m
{
	for (NSMenuItem *i in m.itemArray) {
		if (i.submenu) [self setTargets:i.submenu];
		else if (i.action == @selector(menuCommand:)) i.target = self;
	}
}

- (void)closeForGood
{
	// the temporary folder of this run (empty books, files given to the engines)
	std::error_code ec;
	std::filesystem::remove_all(ng::narrow(self.app->temp_dir_), ec);
	for (auto &[id, t] : timers_) [t invalidate];
	timers_.clear();
	[self.window orderOut:nil];
	[NSApp stop:nil];
	// (stop takes effect after an event)
	[NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0 timestamp:0
	                                windowNumber:0 context:nil subtype:0 data1:0 data2:0]
	         atStart:YES];
}

@end

@implementation NgWindowDelegate
- (BOOL)windowShouldClose:(NSWindow *)w
{
	self.app->close_window();
	return NO;
}
- (void)windowDidBecomeKey:(NSNotification *)n
{
	if (self.app->record_resumes()) {
		NgRecordBox *b = (__bridge NgRecordBox *)self.app->record_edit_;
		[b.field.window makeFirstResponder:b.field];
	}
}
- (void)windowDidMiniaturize:(NSNotification *)n { self.app->update_animation_timer(); }
- (void)windowDidDeminiaturize:(NSNotification *)n
{
	self.app->update_animation_timer();
	self.app->invalidate();
}
- (id)windowWillReturnFieldEditor:(NSWindow *)w toObject:(id)client
{
	NgRecordBox *b = self.app->record_edit_ ? (__bridge NgRecordBox *)self.app->record_edit_ : nil;
	if (!b || client != b.field) return nil;
	if (!self.recordEditor) {
		NgRecordEditor *ed = [[NgRecordEditor alloc] initWithFrame:NSZeroRect];
		ed.fieldEditor = YES;
		ed.box = b;
		self.recordEditor = ed;
	}
	return self.recordEditor;
}
@end

@interface NgAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, assign) ng::App *app;
@end

@implementation NgAppDelegate
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
	// (as the close button: the settings saved and the engines closed first, then the window and the app)
	self.app->close_window();
	return NSTerminateCancel;
}
- (void)applicationDidBecomeActive:(NSNotification *)n
{
	self.app->active_ = true;
	self.app->update_animation_timer();
}
- (void)applicationDidResignActive:(NSNotification *)n
{
	self.app->active_ = false;
	self.app->update_animation_timer();
}
@end

namespace ng {

namespace {

NgWindowDelegate *g_window_delegate = nil;
NgAppDelegate *g_app_delegate = nil;

}  // namespace

int App::run(HINSTANCE, int)
{
	@autoreleasepool {
		[NSApplication sharedApplication];
		NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
		g_app_delegate = [[NgAppDelegate alloc] init];
		g_app_delegate.app = this;
		NSApp.delegate = g_app_delegate;

		prepare();

		const int w = ini_.get_int("window", "w", 1280, 400, 16000);
		const int h = ini_.get_int("window", "h", 820, 300, 16000);
		const int x = ini_.get_int("window", "x", -32001, -32000, 32000);
		const int y = ini_.get_int("window", "y", -32001, -32000, 32000);
		NSWindow *win = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, w, h)
		                                            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
		                                                      NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
		                                              backing:NSBackingStoreBuffered
		                                                defer:NO];
		win.releasedWhenClosed = NO;
		win.title = ns(lang::t("app.title"));
		win.contentMinSize = NSMakeSize(480, 360);
		// the place kept in config.ini (its top left from the top left of the main screen), when it is on a screen
		bool placed = false;
		if (x > -32001 && y > -32001) {
			const NSRect screen = NSScreen.screens.firstObject.frame;
			const NSRect f = NSMakeRect(x, NSMaxY(screen) - y - h, w, h);
			for (NSScreen *s in NSScreen.screens)
				if (NSIntersectsRect(s.visibleFrame, f)) placed = true;
			if (placed) [win setFrame:f display:NO];
		}
		if (!placed) [win center];
		NgView *view = [[NgView alloc] initWithApp:this frame:NSMakeRect(0, 0, w, h)];
		win.contentView = view;
		g_window_delegate = [[NgWindowDelegate alloc] init];
		g_window_delegate.app = this;
		win.delegate = g_window_delegate;
		hwnd_ = (__bridge void *)view;

		if (!gfx_.init(hwnd_)) {
			message(lang::t("err.d2d"), MB_ICONERROR);
			return 1;
		}
		create_record_box();
		build_menu();
		set_window_icon();
		gfx_.resize();
		layout();
		[win makeKeyAndOrderFront:nil];
		[win makeFirstResponder:view];
		if (ini_.get_bool("window", "max", false)) [win zoom:nil];
		[NSApp activateIgnoringOtherApps:YES];
		started();
		[NSApp run];
		hwnd_ = nullptr;
	}
	return 0;
}

void App::set_window_icon() {}  // (the icon of the app is in its bundle)

void App::set_timer(UINT_PTR id, UINT ms)
{
	if (hwnd_) [(__bridge NgView *)hwnd_ setTimer:id ms:ms];
}

void App::kill_timer(UINT_PTR id)
{
	if (hwnd_) [(__bridge NgView *)hwnd_ killTimer:id];
}

void App::post(UINT msg) { post_message(hwnd_, msg); }

bool App::window_enabled() const { return !ng_modal_open(); }

std::wstring App::pick_open(const wchar_t *, const std::wstring &title)
{
	NSOpenPanel *p = [NSOpenPanel openPanel];
	p.canChooseFiles = YES;
	p.canChooseDirectories = NO;
	p.allowsMultipleSelection = NO;
	if (!title.empty()) p.message = ns(title);
	ng_modal_begin();
	const NSModalResponse r = [p runModal];
	ng_modal_end();
	if (r != NSModalResponseOK || !p.URL) return {};
	return from_ns(p.URL.path);
}

std::wstring App::pick_save(const wchar_t *, const wchar_t *ext, const std::wstring &name, const std::wstring &title)
{
	NSSavePanel *p = [NSSavePanel savePanel];
	const size_t slash = name.find_last_of(L"/");
	if (slash != std::wstring::npos) {
		p.directoryURL = [NSURL fileURLWithPath:ns(name.substr(0, slash)) isDirectory:YES];
		p.nameFieldStringValue = ns(name.substr(slash + 1));
	} else if (!name.empty()) {
		p.nameFieldStringValue = ns(name);
	}
	if (!title.empty()) p.message = ns(title);
	p.extensionHidden = NO;
	ng_modal_begin();
	const NSModalResponse r = [p runModal];
	ng_modal_end();
	if (r != NSModalResponseOK || !p.URL) return {};
	std::wstring path = from_ns(p.URL.path);
	// (a name without an extension gets the one of the kind of file)
	if (ext && *ext) {
		const size_t s = path.find_last_of(L"/"), dot = path.find_last_of(L'.');
		if (dot == std::wstring::npos || (s != std::wstring::npos && dot < s)) path += L"." + std::wstring(ext);
	}
	return path;
}

}  // namespace ng
