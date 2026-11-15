// macOS: the view of the main window (src/mac/app_mac.mm). The App draws into a picture kept by Gfx (src/mac/gfx_mac.mm);
// the view puts that picture on the screen, and hands the events of the mouse, the keys, the menus and the timers to
// the App.
#pragma once

#import <Cocoa/Cocoa.h>

#include <cstdint>

namespace ng {
class App;
}

@interface NgView : NSView
// the App draws the whole window when the view is drawn next (App::paint)
@property(nonatomic) BOOL needsPaint;
// the menus of the window (the menu bar of the app, after the menu of the app itself)
@property(nonatomic, strong) NSMenu *menuBar;
- (instancetype)initWithApp:(ng::App *)app frame:(NSRect)frame;
- (void)handleMessage:(unsigned)msg wParam:(std::uintptr_t)wp lParam:(std::intptr_t)lp;
- (void)menuCommand:(id)sender;
// a small menu at (x, y) of the view; the command chosen (0: none) is returned, not run
- (int)popUpMenu:(NSMenu *)menu atX:(int)x y:(int)y;
// the window goes and the app ends (after App::close_window)
- (void)closeForGood;
- (void)setTimer:(std::uintptr_t)tid ms:(unsigned)ms;
- (void)killTimer:(std::uintptr_t)tid;
@end

// a modal window is open over the main window (App::window_enabled is false meanwhile)
void ng_modal_begin();
void ng_modal_end();
bool ng_modal_open();
