#include "windows_to_utf8.h"

#include "_windows.h"
#include "assert.h"

std::string utf16_to_utf8 (const wchar_t* in) {
    int len = WideCharToMultiByte(CP_UTF8, 0, in, -1, nullptr, 0, nullptr, nullptr);
    std::string r (len-1, 0);
    auto buf = const_cast<char*>(r.data());
    AW(WideCharToMultiByte(CP_UTF8, 0, in, -1, buf, len, nullptr, nullptr));
    return r;
}
