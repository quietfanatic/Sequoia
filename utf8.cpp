#include "utf8.h"

#include <windows.h>
#include "assert.h"

std::string to_utf8 (const wchar_t* in) {
    if (!in[0]) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, in, -1, nullptr, 0, nullptr, nullptr);
    AW(len);
    std::string r (len-1, 0);
    auto buf = const_cast<char*>(r.data());
    AW(WideCharToMultiByte(CP_UTF8, 0, in, -1, buf, len, nullptr, nullptr));
    return r;
}

std::wstring to_utf16 (const char* in) {
    if (!in[0]) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, in, -1, nullptr, 0);
    AW(len);
    std::wstring r (len-1, 0);
    auto buf = const_cast<wchar_t*>(r.data());
    AW(MultiByteToWideChar(CP_UTF8, 0, in, -1, buf, len));
    return r;
}
