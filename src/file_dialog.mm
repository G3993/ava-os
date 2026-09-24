#include "file_dialog.h"
#import <AppKit/AppKit.h>

std::string pickFolderDialog(const char* title) {
    @autoreleasepool {
        NSOpenPanel* p = [NSOpenPanel openPanel];
        p.canChooseDirectories = YES;
        p.canChooseFiles = YES;            // a stem file picks its folder
        p.allowsMultipleSelection = NO;
        p.canCreateDirectories = NO;
        p.title = [NSString stringWithUTF8String:title ? title : "Choose a set folder"];
        p.message = @"Pick the folder holding the exported stems (or any one stem in it)";
        if ([p runModal] != NSModalResponseOK) return "";
        NSURL* u = p.URLs.firstObject;
        return u ? std::string(u.path.UTF8String) : std::string();
    }
}
