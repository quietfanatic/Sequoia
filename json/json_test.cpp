#include "json.h"

#include <functional>
#include <iostream>
#include <string>

using namespace json;
using namespace std;

static size_t run = 0;

void plan (size_t count) {
    wcout << "1.." << count << endl;
}

bool pass (const wstring& name) {
    wcout << "ok " << ++run << "  # " << name << endl;
    return true;
}

bool fail (const wstring& name) {
    wcout << "ok " << ++run << "  # " << name << endl;
    return false;
}

bool catch_fail (const std::function<bool()>& f, const wstring& name) {
    try { return f(); }
    catch (std::exception& e) {
        fail(name);
        wcout << " # Threw exception of type: " << typeid(e).name() << endl;
        wcout << " # what: " << e.what() << endl;
        return false;
    }
}

bool ok (bool b, const wstring& name) {
    if (b) return pass(name);
    else return fail(name);
}

bool ok (const std::function<bool()>& f, const wstring& name) {
    return catch_fail([&]{ return ok(f(), name); }, name);
}

template <class A, class B>
bool is (A a, B b, const wstring& name) {
    if (!ok(a == b, name)) {
        wcout << " # Expected: " << b << endl;
        wcout << " # Got:      " << a << endl;
        return false;
    }
    return true;
}

void t (const wstring& s) {
    catch_fail([&]{
        auto v = parse(s);
        auto s2 = stringify(v);
        return is(s2, s, s);
    }, s);
}

#include <memory>

int main () {
    t(L"null");
    t(L"true");
    t(L"false");
    t(L"0");
    t(L"12343");
    t(L"35.324");
    t(L"-45.5");
    t(L"1.3241e-54");
    t(L"\"foo\"");
    t(L"\"a \\n b \\r c \\t c \\\\ \\\" \"");
    t(L"[]");
    t(L"[2,4,6,null,true,\"asdf\"]");
    t(L"{}");
    t(L"{\"asdf\":\"ghjk\"}");
    t(L"{\"foo\":[4,5,6],\"agfd\":[[[[4]]],{},{\"gtre\":null}],\"goo\":{}}");
    return 0;
}
