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
        String string;
        Array array;
        Object object;
    };

    Value (nullptr_t v = nullptr) : type(NULL) { }
    Value (bool v) : type(BOOL), boolean(v) { }
    Value (double v) : type(NUMBER), number(v) { }
    Value (const String& v) : type(STRING), string(v) { }
    Value (String&& v) : type(STRING), string(v) { }
    Value (const Array& v) : type(ARRAY), array(v) { }
    Value (Array&& v) : type(ARRAY), array(v) { }
    Value (const Object& v) : type(OBJECT), object(v) { }
    Value (Object&& v) : type(OBJECT), object(v) { }

    Value (const Value& v);

    Value (Value&& v);

    Value& operator= (const Value& v) {
        this->~Value();
        new (this) Value(v);
        return *this;
    }
    Value& operator= (Value&& v) {
        this->~Value();
        new (this) Value(v);
        return *this;
    }

    ~Value ();
};

String stringify (const Value& v);

Value parse (const String& s);

} // namespace json

