#pragma once
#include <string>
#include <utility>
#include <vector>

namespace json {

enum Type {
    NULLTYPE = 0, // Secretly you can use NULL for this
    BOOL,
    NUMBER,
    STRING,
    ARRAY,
    OBJECT
};

struct Value;

using Char = wchar_t;
using String = std::wstring;
using Array = std::vector<Value>;
using Object = std::vector<std::pair<String, Value>>;

struct Value {
    unsigned char type;
    union {
        bool boolean;
        double number;
        String* string;
        Array* array;
        Object* object;
    };

    Value (nullptr_t v = nullptr) : type(NULL) { }
    Value (bool v) : type(BOOL), boolean(v) { }
    Value (double v) : type(NUMBER), number(v) { }
    Value (const Char* v) : type(STRING), string(new String(v)) { }
    Value (const String& v) : type(STRING), string(new String(v)) { }
    Value (String&& v) : type(STRING), string(new String(std::move(v))) { }
    Value (const Array& v) : type(ARRAY), array(new Array(std::move(v))) { }
    Value (Array&& v) : type(ARRAY), array(new Array(std::move(v))) { }
    Value (const Object& v) : type(OBJECT), object(new Object(std::move(v))) { }
    Value (Object&& v) : type(OBJECT), object(new Object(std::move(v))) { }

    Value (String*&& v) : type(STRING), string(v) { v = nullptr; }
    Value (Array*&& v) : type(ARRAY), array(v) { v = nullptr; }
    Value (Object*&& v) : type(OBJECT), object(v) { v = nullptr; }

    Value (const Value& v);
    Value (Value&& v);

    Value& operator= (const Value& v) {
        this->~Value();
        new (this) Value(v);
        return *this;
    }
    Value& operator= (Value&& v) {
        this->~Value();
        new (this) Value(std::move(v));
        return *this;
    }

    Value& operator[] (size_t i) { return (*array)[i]; }
    const Value& operator[] (size_t i) const { return (*array)[i]; }

    Value& operator[] (const String& key) {
        static Value nothing;
        for (auto& p : *object) {
            if (p.first == key) return p.second;
        }
        return nothing;
    }
    const Value& operator[] (const String& key) const {
        static Value nothing;
        for (auto& p : *object) {
            if (p.first == key) return p.second;
        }
        return nothing;
    }

    ~Value ();
};

bool operator== (const Value& a, const Value& b);
inline bool operator!= (const Value& a, const Value& b) { return !(a == b); }

String stringify (const Value& v);

Value parse (const String& s);
Value parse (const Char* s);

} // namespace json

