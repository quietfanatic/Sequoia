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
    case STRING: string = new String(*v.string); break;
    case ARRAY: array = new Array(*v.array); break;
    case OBJECT: object = new Object(*v.object); break;
    }
}

Value::Value (Value&& v) : type(v.type) {
    switch (type) {
    case BOOL: boolean = v.boolean; break;
    case NUMBER: number = v.number; break;
    case STRING: string = v.string; v.string = nullptr; break;
    case ARRAY: array = v.array; v.array = nullptr; break;
    case OBJECT: object = v.object; v.object = nullptr; break;
    }
    v.type = NULL;
}

Value::~Value () {
    switch (type) {
    case STRING: delete string; break;
    case ARRAY: delete array; break;
    case OBJECT: delete object; break;
    }
}

bool operator== (const Value& a, const Value& b) {
    if (a.type != b.type) return false;
    switch (a.type) {
        case NULL: return true;
        case BOOL: return a.boolean == b.boolean;
        case NUMBER: return a.number == b.number;
        case STRING: return *a.string == *b.string;
        case ARRAY: {
            if (a.array->size() != b.array->size()) return false;
            for (size_t i = 0; i < a.array->size(); i++) {
                if ((*a.array)[i] != (*b.array)[i]) return false;
            }
            return true;
        }
        case OBJECT: {
            if (a.object->size() != b.object->size()) return false;
            for (auto& ae : *a.object) {
                for (auto& be : *b.object) {
                    if (ae.first == be.first) {
                        if (ae.second != be.second) return false;
                        goto next_ae;
                    }
                    return false;
                }
                next_ae: { }
            }
            return true;
        }
        default: throw std::logic_error("Invalid json::Value type");
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
            case '"': return s;  //"
            case '\\': {
                switch (c = get()) {
                case 0: throw error();
                case 'b': s += '\b'; break;
                case 'f': s += '\f'; break;
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
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
        case 'n': {
            if (get() == 'u' && get() == 'l' && get() == 'l') {
                return nullptr;
            }
            else throw error();
        }
        case 't': {
            if (get() == 'r' && get() == 'u' && get() == 'e') {
                return true;
            }
            else throw error();
        }
        case 'f': {
            if (get() == 'a' && get() == 'l' && get() == 's' && get() == 'e') {
                return false;
            }
            else throw error();
        }
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
        case '-': {
            pos -= 1;
            return strtod(pos, const_cast<Char**>(&pos));
        }
        case '"': {  //"
            return str();
        }
        case '[': {
            ws();
            Array a;
            if (*pos == ']') {
                get(); return std::move(a);
            }
            while (1) {
                a.emplace_back(value());
                ws();
                switch (get()) {
                case ']': return std::move(a);
                case ',': break;
                default: throw error();
                }
            }
        }
        case '{': {
            ws();
            Object o;
            if (*pos == '}') {
                get(); return std::move(o);
            }
            while (1) {
                if (get() != '"') throw error();  //"
                String key = str();
                ws();
                if (get() != ':') throw error();
                o.emplace_back(key, value());
                ws();
                switch (get()) {
                case '}': return std::move(o);
                case ',': break;
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
Value parse (const Char* s) {
    const Char* end = s;
    while (*end != 0) end++;
    return Parser{s, end}.parse();
}

String stringify (const Value& v) {
    switch (v.type) {
    case NULL: return "null";
    case BOOL: return v.boolean ? "true" : "false";
    case NUMBER: {
        Char buf [32];
        snprintf(buf, 32, "%g", v.number);
        return buf;
    }
    case STRING: {
         // Not particularly efficient
        String r = "\"";
        for (auto c : *v.string)
        switch (c) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\b': r += "\\b"; break;
        case '\f': r += "\\f"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default: r += c; break;
        }
        return r + "\"";
    }
    case ARRAY: {
        String r = "[";
        for (auto& e : *v.array) {
            if (&e != &v.array->front()) {
                r += ",";
            }
            r += stringify(e);
        }
        return r + "]";
    }
    case OBJECT: {
        String r = "{";
        for (auto& e : *v.object) {
            if (&e != &v.object->front()) {
                r += ",";
            }
            r += stringify(e.first);
            r += ":";
            r += stringify(e.second);
        }
        return r + "}";
    }
    default: throw std::logic_error("Invalid json::Value type");
    }
}

} // namespace json

