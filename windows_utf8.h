#pragma once

#include <string>

std::string to_utf8 (const wchar_t*);
static inline std::string to_utf8 (const std::wstring& s) { return to_utf8(s.c_str()); }

std::wstring to_utf16 (const char*);
static inline std::wstring to_utf16 (const std::string& s) { return to_utf16(s.c_str()); }
