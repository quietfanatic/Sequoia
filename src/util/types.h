#pragma once

#include <stdint.h>
#include <string>
#include <string_view>

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using uint = unsigned int;

using String = std::string;
using String16 = std::wstring;
using Str = std::string_view;
using Str16 = std::wstring_view;

#pragma warning(push)
 // Yeah, I know these are reserved.  They're reserved for THIS EXACTLY.
#pragma warning(disable:4455)
using std::literals::operator""s;
using std::literals::operator""sv;
#pragma warning(pop)

 // Why these aren't standard I don't know.
static inline String operator + (Str a, Str b) {
    String r;
    r.reserve(a.size() + b.size());
    r.append(a);
    r.append(b);
    return r;
}
static inline String16 operator + (Str16 a, Str16 b) {
    String16 r;
    r.reserve(a.size() + b.size());
    r.append(a);
    r.append(b);
    return r;
}
static inline String& operator += (String& a, Str b) { return a.append(b); }
static inline String16& operator += (String16& a, Str16 b) { return a.append(b); }
