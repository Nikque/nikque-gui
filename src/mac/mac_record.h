// macOS: the text box of the row of the moves (src/mac/app_record_mac.mm), kept in App::record_edit_.
#pragma once

#import <Cocoa/Cocoa.h>

namespace ng {
class App;
}

@interface NgRecordBox : NSObject <NSTextFieldDelegate>
@property(nonatomic, assign) ng::App *app;
@property(nonatomic, strong) NSTextField *field;
@property(nonatomic) BOOL modified;  // typed in since it was given the moves
// a paste in the box (the field editor of the window sends it here)
- (void)paste;
@end
