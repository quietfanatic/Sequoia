#pragma once

#include <string>

std::string to_utf8 (const wchar_t*);
static inline std::string to_utf8 (const std::wstring& s) { return to_utf8(s.c_str()); }
static inline const std::string& to_utf8 (const std::string& s) { return s; }
static inline std::string to_utf8 (const char* s) { return s; }

std::wstring to_utf16 (const char*);
static inline std::wstring to_utf16 (const std::string& s) { return to_utf16(s.c_str()); }
static inline const std::wstring& to_utf16 (const std::wstring& s) { return s; }
static inline std::wstring to_utf16 (const wchar_t* s) { return s; }
