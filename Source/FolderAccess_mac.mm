#import <Foundation/Foundation.h>
#include "FolderAccess.h"
static std::string encode(NSURL* u) {
    NSError* error = nil;
    NSData* data = [u bookmarkDataWithOptions:(NSURLBookmarkCreationWithSecurityScope | NSURLBookmarkCreationSecurityScopeAllowOnlyReadAccess)
                  includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
    return data ? std::string([[data base64EncodedStringWithOptions:0] UTF8String]) : std::string();
}
std::string FolderAccess::makeBookmark(const std::string& p) {
    @autoreleasepool { return encode([NSURL fileURLWithPath:[NSString stringWithUTF8String:p.c_str()] isDirectory:YES]); }
}
FolderAccess::FolderAccess(const std::string& p, const std::string& b) : path(p), bookmark(b) {
    @autoreleasepool {
        NSURL* u = nil;
        if (!b.empty()) {
            NSData* data = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:b.c_str()] options:0];
            BOOL stale = NO;
            u = [NSURL URLByResolvingBookmarkData:data options:(NSURLBookmarkResolutionWithSecurityScope | NSURLBookmarkResolutionWithoutUI)
                 relativeToURL:nil bookmarkDataIsStale:&stale error:nullptr];
            [data release];
            if (u) {
                scoped = [u startAccessingSecurityScopedResource];
                path = std::string([[u path] UTF8String]);
                if (stale) { auto updated = encode(u); if (!updated.empty()) bookmark = updated; }
            }
        }
        url = [u retain];
    }
}
FolderAccess::~FolderAccess() {
    @autoreleasepool { NSURL* u = (NSURL*)url; if (scoped) [u stopAccessingSecurityScopedResource]; [u release]; }
}
