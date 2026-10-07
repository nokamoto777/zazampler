#include "FolderAccess.h"
FolderAccess::FolderAccess(const std::string& p, const std::string& b) : path(p), bookmark(b) {}
FolderAccess::~FolderAccess() = default;
std::string FolderAccess::makeBookmark(const std::string&) { return {}; }
