#include "text.h"

#include <windows.h>
#include "assert.h"

using namespace std;

string from_utf16 (Str16 in) {
    if (in.empty()) return ""s;
    int len = WideCharToMultiByte(CP_UTF8, 0, in.data(), -1, nullptr, 0, nullptr, nullptr);
    AW(len);
    string r (len-1, 0);
    auto buf = const_cast<char*>(r.data());
    AW(WideCharToMultiByte(CP_UTF8, 0, in.data(), -1, buf, len, nullptr, nullptr));
    return r;
}

wstring to_utf16 (Str in) {
    if (in.empty()) return L""s;
    int len = MultiByteToWideChar(CP_UTF8, 0, in.data(), -1, nullptr, 0);
    AW(len);
    wstring r (len-1, 0);
    auto buf = const_cast<wchar_t*>(r.data());
    AW(MultiByteToWideChar(CP_UTF8, 0, in.data(), -1, buf, len));
    return r;
}

char to_hex_digit (uint8 nyb) {
    AA(nyb < 16);
    switch (nyb) {
    default: return '0' + nyb;
    case 0xa:
    case 0xb:
    case 0xc:
    case 0xd:
    case 0xe:
    case 0xf: return 'A' + nyb - 0xa;
    }
}

int8 from_hex_digit (char c) {
    switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        return c - '0';
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
        return c - 'A' + 0xa;
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
        return c - 'a' + 0xa;
    default: return -1;
    }
}

string escape_url (Str input) {
    string r;
    for (auto c : input) {
        switch (c) {
        case ' ': r += '+'; break;
        case '+':
        case '&':
        case ';':
        case '=':
        case '?':
        case '#': {
            r += '%' + to_hex_digit(uint8(c) % 16)
                     + to_hex_digit(uint8(c) / 16);
            break;
        }
        default: r += c; break;
        }
    }
    return r;
}

string make_url_utf8 (Str url) {
    string r;
    for (size_t i = 0; i < url.size(); i++) {
        if (url[i] == '%' && i + 2 < url.size()) {
            int8 high = from_hex_digit(url[i+1]);
            int8 low = from_hex_digit(url[i+2]);
            if (high >= 0 && low >= 0) {
                uint8 val = uint8(high) * 16 + uint8(low);
                 // Only unescape UTF-8 surrogates
                if (val >= 0x80) {
                    r += char(val);
                    i += 2; continue;
                }
            }
        }
        r += url[i];
    }
    return r;
}
