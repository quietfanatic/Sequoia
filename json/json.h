#pragma once
#include <cassert>
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

using Char = char;
using String = std::string;
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
    Value (signed char v) : type(NUMBER), number(double(v)) { }
    Value (unsigned char v) : type(NUMBER), number(double(v)) { }
    Value (short v) : type(NUMBER), number(double(v)) { }
    Value (unsigned short v) : type(NUMBER), number(double(v)) { }
    Value (int v) : type(NUMBER), number(double(v)) { }
    Value (unsigned int v) : type(NUMBER), number(double(v)) { }
    Value (long v) : type(NUMBER), number(double(v)) { }
    Value (unsigned long v) : type(NUMBER), number(double(v)) { }
    Value (long long v) : type(NUMBER), number(double(v)) { }
    Value (unsigned long long v) : type(NUMBER), number(double(v)) { }
    Value (float v) : type(NUMBER), number(double(v)) { }
    Value (double v) : type(NUMBER), number(double(v)) { }
    Value (const Char* v) : type(STRING), string(new String(v)) { }
    Value (const String& v) : type(STRING), string(new String(v)) { }
    Value (String&& v) : type(STRING), string(new String(std::move(v))) { }
    Value (const Array& v) : type(ARRAY), array(new Array(std::move(v))) { }
    Value (Array&& v) : type(ARRAY), array(new Array(std::move(v))) { }
    Value (const Object& v) : type(OBJECT), object(new Object(std::move(v))) { }
    Value (Object&& v) : type(OBJECT), object(new Object(std::move(v))) { }
    Value (const Value& v);
    Value (Value&& v);

    explicit Value (String*&& v) : type(STRING), string(v) { v = nullptr; }
    explicit Value (Array*&& v) : type(ARRAY), array(v) { v = nullptr; }
    explicit Value (Object*&& v) : type(OBJECT), object(v) { v = nullptr; }

    template <class T>
    Value (T* v) { static_assert(false, "Can't convert this pointer to json::Value"); }

    operator const nullptr_t& () const { assert(type == NULL); return nullptr; }
    operator const bool& () const { assert(type == BOOL); return boolean; }
    operator signed char () const { assert(type == NUMBER); return signed char(number); }
    operator unsigned char () const { assert(type == NUMBER); return unsigned char(number); }
    operator short () const { assert(type == NUMBER); return short(number); }
    operator unsigned short () const { assert(type == NUMBER); return unsigned short(number); }
    operator int () const { assert(type == NUMBER); return int(number); }
    operator unsigned int () const { assert(type == NUMBER); return unsigned int(number); }
    operator long () const { assert(type == NUMBER); return long(number); }
    operator unsigned long () const { assert(type == NUMBER); return unsigned long(number); }
    operator long long () const { assert(type == NUMBER); return long long(number); }
    operator unsigned long long () const { assert(type == NUMBER); return unsigned long long(number); }
    operator float () const { assert(type == NUMBER); return float(number); }
    operator const double& () const { assert(type == NUMBER); return number; }
    operator const String& () const { assert(type == STRING); return *string; }
    operator const Array& () const { assert(type == ARRAY); return *array; }
    operator const Object& () const { assert(type == OBJECT); return *object; }

    template <class T> operator const T& () const {
        static_assert(false, "Can't convert json::Value to this type.");
    }

    Value& operator[] (size_t i) {
        assert(type == ARRAY && array->size() > i);
        return (*array)[i];
    }
    const Value& operator[] (size_t i) const {
        assert(type == ARRAY && array->size() > i);
        return (*array)[i];
    }

    Value& operator[] (const String& key) {
        assert(type == OBJECT);
        for (auto& p : *object) {
            if (p.first == key) return p.second;
        }
        assert(false);
        static Value nothing;
        return nothing;
    }
    const Value& operator[] (const String& key) const {
        return const_cast<const Value&>(const_cast<Value&>(*this)[key]);
    }

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

    ~Value ();
};

bool operator== (const Value& a, const Value& b);
inline bool operator!= (const Value& a, const Value& b) { return !(a == b); }

String stringify (const Value& v);

Value parse (const String& s);
Value parse (const Char* s);

} // namespace json

