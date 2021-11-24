#pragma once

#include <cassert>
#include <string>
#include <string_view>
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
using Str = std::string_view;
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
    Value (Str v) : type(STRING), string(new String(v)) { }
    Value (String&& v) : type(STRING), string(new String(std::move(v))) { }
    Value (const Array& v) : type(ARRAY), array(new Array(std::move(v))) { }
    Value (Array&& v) : type(ARRAY), array(new Array(std::move(v))) { }
    Value (const Object& v) : type(OBJECT), object(new Object(std::move(v))) { }
    Value (Object&& v) : type(OBJECT), object(new Object(std::move(v))) { }
    Value (const Value& v);
    Value (Value&& v);

     // Mostly an optimization for internal use, but you can use them if you know what you're doing
    explicit Value (String*&& v) : type(STRING), string(v) { v = nullptr; }
    explicit Value (Array*&& v) : type(ARRAY), array(v) { v = nullptr; }
    explicit Value (Object*&& v) : type(OBJECT), object(v) { v = nullptr; }

    template <class T>
    Value (T* v) { static_assert(false, "Can't convert this pointer to json::Value"); }

    template <class T>
    bool is () const { static_assert(false, "Can't call json::Value::is<>() with this type because it isn't a type json::Value can be casted to."); }
    template <> bool is<bool> () const { return type == BOOL; }
    template <> bool is<signed char> () const { return type == NUMBER; }
    template <> bool is<unsigned char> () const { return type == NUMBER; }
    template <> bool is<short> () const { return type == NUMBER; }
    template <> bool is<unsigned short> () const { return type == NUMBER; }
    template <> bool is<int> () const { return type == NUMBER; }
    template <> bool is<unsigned int> () const { return type == NUMBER; }
    template <> bool is<long> () const { return type == NUMBER; }
    template <> bool is<unsigned long> () const { return type == NUMBER; }
    template <> bool is<long long> () const { return type == NUMBER; }
    template <> bool is<unsigned long long> () const { return type == NUMBER; }
    template <> bool is<String> () const { return type == STRING; }
    template <> bool is<Array> () const { return type == ARRAY; }
    template <> bool is<Object> () const { return type == OBJECT; }

     // General casting
    operator bool () const { assert(type == BOOL); return boolean; }
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
    operator double () const { assert(type == NUMBER); return number; }
    operator Str () const { assert(type == STRING); return *string; }
    operator const String& () const& { assert(type == STRING); return *string; }
    operator String&& () && { assert(type == STRING); return std::move(*string); }
    operator const Array& () const& { assert(type == ARRAY); return *array; }
    operator Array&& () && { assert(type == ARRAY); return std::move(*array); }
    operator const Object& () const& { assert(type == OBJECT); return *object; }
    operator Object&& () && { assert(type == OBJECT); return std::move(*object); }

//    template <class T> operator const T& () const {
//        static_assert(false, "Can't cast json::Value to this type.");
//    }

    bool has (size_t i) const {
        return(type == ARRAY && array->size() > i);
    }

    Value& operator[] (size_t i) & {
        assert(has(i));
        return (*array)[i];
    }
    const Value& operator[] (size_t i) const& {
        return const_cast<const Value&>(const_cast<Value&>(*this)[i]);
    }
    Value&& operator[] (size_t i) && {
        return std::move(const_cast<Value&>(*this)[i]);
    }

    bool has (Str key) {
        if (type != OBJECT) return false;
        for (auto& p : *object) {
            if (p.first == key) return true;
        }
        return false;
    }

    Value& operator[] (Str key) & {
        assert(type == OBJECT);
        for (auto& p : *object) {
            if (p.first == key) return p.second;
        }
        assert(false);
        static Value nothing;
        return nothing;
    }
    const Value& operator[] (Str key) const& {
        return const_cast<const Value&>(const_cast<Value&>(*this)[key]);
    }
    Value&& operator[] (Str key) && {
        return std::move(const_cast<Value&>(*this)[key]);
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

 // There's no way to make json::Array{...} use move semantics, so here's
 // some helper functions to avoid unnecessary copies.
template <class... Args>
Array array (Args&&... args) {
    Array r;
    r.reserve(sizeof...(args));
    (r.emplace_back(std::forward<Args>(args)), ...);
    return r;
}
template <class... Args>
Object object (std::pair<Str, Args>&&... args) {
    Object r;
    r.reserve(sizeof...(args));
    (r.emplace_back(std::move(args)), ...);
    return r;
}

bool operator== (const Value& a, const Value& b);
inline bool operator!= (const Value& a, const Value& b) { return !(a == b); }

String stringify (const Value& v);

Value parse (Str s);

} // namespace json

