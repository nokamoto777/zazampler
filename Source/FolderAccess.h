#pragma once
#include <string>
// A directory bookmark grants recursive read access to SFZ and relative samples.
// macOS implementation retains access for the entire streaming lifetime.
class FolderAccess {
public:
    FolderAccess(const std::string& fallbackPath, const std::string& base64Bookmark);
    ~FolderAccess();
    FolderAccess(const FolderAccess&) = delete;
    FolderAccess& operator=(const FolderAccess&) = delete;
    static std::string makeBookmark(const std::string& path);
    std::string path;
    std::string bookmark;
private:
    void* url = nullptr;
    bool scoped = false;
};
