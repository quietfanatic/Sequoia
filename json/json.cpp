#include "json.h"

#include <stdexcept>

#include <iomanip>
#include <iostream>
using namespace std;

namespace json {

Value::Value (const Value& v) : type(v.type) {
    switch (type) {
    case BOOL: boolean = v.boolean; break;
    case NUMBER: number = v.number; break;
    case STRING: new (&string) String(v.string); break;
    case ARRAY: new (&array) Array(v.array); break;
    case OBJECT: new (&object) Object(v.object); break;
    }
}

Value::Value (Value&& v) : type(v.type) {
    switch (type) {
    case BOOL: boolean = v.boolean; break;
    case NUMBER: number = v.number; break;
    case STRING: new (&string) String(std::move(v.string)); break;
    case ARRAY: new (&array) Array(std::move(v.array)); break;
    case OBJECT: new (&object) Object(std::move(v.object)); break;
    }
    v.type = NULL;
}

Value::~Value () {
    switch (type) {
    case STRING: string.~String(); break;
    case ARRAY: array.~Array(); break;
    case OBJECT: object.~Object(); break;
    }
}

namespace {
struct Parser {
    const Char* pos;
    const Char* end;
    std::logic_error error () { return std::logic_error("Syntax error in JSON"); }
    Char get () {
        if (pos == end) return 0;
        return *pos++;
    }
    void ws () {
        while (iswspace(*pos)) get();
    }
    String str () {
        String s;
        while (Char c = get()) {
            switch (c) {
            case L'"': return s;  //"
            case L'\\': {
                switch (c = get()) {
                case 0: throw error();
                case L'b': s += L'\b'; break;
                case L'f': s += L'\f'; break;
                case L'n': s += L'\n'; break;
                case L'r': s += L'\r'; break;
                case L't': s += L'\t'; break;
                default: s += c; break;
                }
                break;
            }
            default: s += c; break;
            }
        }
        throw error();
    }
    Value value () {
        ws();
        switch (get()) {
        case L'n': {
            if (get() == L'u' && get() == L'l' && get() == L'l') {
                return nullptr;
            }
            else throw error();
        }
        case L't': {
            if (get() == L'r' && get() == L'u' && get() == L'e') {
                return true;
            }
            else throw error();
        }
        case L'f': {
            if (get() == L'a' && get() == L'l' && get() == L's' && get() == L'e') {
                return false;
            }
            else throw error();
        }
        case L'0':
        case L'1':
        case L'2':
        case L'3':
        case L'4':
        case L'5':
        case L'6':
        case L'7':
        case L'8':
        case L'9':
        case L'-': {
            pos -= 1;
            return wcstod(pos, const_cast<Char**>(&pos));
        }
        case L'"': {  //"
            return str();
        }
        case L'[': {
            ws();
            Array a;
            if (*pos == L']') {
                get(); return a;
            }
            while (1) {
                a.emplace_back(value());
                ws();
                switch (get()) {
                case L']': return a;
                case L',': break;
                default: throw error();
                }
            }
        }
        case L'{': {
            ws();
            Object o;
            if (*pos == L'}') {
                get(); return o;
            }
            while (1) {
                if (get() != L'"') throw error();  //"
                String key = str();
                ws();
                if (get() != L':') throw error();
                o.emplace_back(key, value());
                ws();
                switch (get()) {
                case L'}': return o;
                case L',': break;
                default: throw error();
                }
            }
        }
        default: throw error();
        }
    }
    Value parse () {
        Value v = value();
        ws();
        if (pos != end) throw error();
        return v;
    }
};
}

Value parse (const String& s) {
    return Parser{s.c_str(), s.c_str() + s.size()}.parse();
}

String stringify (const Value& v) {
    switch (v.type) {
    case NULL: return L"null";
    case BOOL: return v.boolean ? L"true" : L"false";
    case NUMBER: {
        Char buf [32];
        swprintf(buf, 32, L"%g", v.number);
        return buf;
    }
    case STRING: {
         // Not particularly efficient
        String r = L"\"";
        for (auto c : v.string)
        switch (c) {
        case L'"': r += L"\\\""; break;
        case L'\\': r += L"\\\\"; break;
        case L'\b': r += L"\\b"; break;
        case L'\f': r += L"\\f"; break;
        case L'\n': r += L"\\n"; break;
        case L'\r': r += L"\\r"; break;
        case L'\t': r += L"\\t"; break;
        default: r += c; break;
        }
        return r + L"\"";
    }
    case ARRAY: {
        String r = L"[";
        for (auto& e : v.array) {
            if (&e != &v.array.front()) {
                r += L",";
            }
            r += stringify(e);
        }
        return r + L"]";
    }
    case OBJECT: {
        String r = L"{";
        for (auto& e : v.object) {
            if (&e != &v.object.front()) {
                r += L",";
            }
            r += stringify(e.first);
            r += L":";
            r += stringify(e.second);
        }
        return r + L"}";
    }
    default: throw std::logic_error("Invalid json::Value type");
    }
}

} // namespace json

