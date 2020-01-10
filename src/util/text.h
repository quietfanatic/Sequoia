#pragma once

#include <string>

#include "types.h"

std::string from_utf16 (const wchar_t*);
static inline std::string from_utf16 (const std::wstring& s) { return from_utf16(s.c_str()); }
static inline const std::string& from_utf16 (const std::string& s) { return s; }
static inline std::string from_utf16 (const char* s) { return s; }

std::wstring to_utf16 (const char*);
static inline std::wstring to_utf16 (const std::string& s) { return to_utf16(s.c_str()); }
static inline const std::wstring& to_utf16 (const std::wstring& s) { return s; }
static inline std::wstring to_utf16 (const wchar_t* s) { return s; }

 // Must be 0..15
char to_hex_digit (uint8 nyb);

 // Returns -1 if not a hex digit
int8 from_hex_digit (char c);

std::string escape_url (const std::string&);
std::string make_url_utf8 (const std::string&);
