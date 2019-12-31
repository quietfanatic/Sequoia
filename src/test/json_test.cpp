#include <functional>
#include <iostream>
#include <string>

#include "../util/json.h"

using namespace json;
using namespace std;

static size_t run = 0;

void plan (size_t count) {
    cout << "1.." << count << endl;
}

bool pass (const string& name) {
    cout << "ok " << ++run << "  # " << name << endl;
    return true;
}

bool fail (const string& name) {
    cout << "ok " << ++run << "  # " << name << endl;
    return false;
}

bool catch_fail (const std::function<bool()>& f, const string& name) {
    try { return f(); }
    catch (std::exception& e) {
        fail(name);
        cout << " # Threw exception of type: " << typeid(e).name() << endl;
        cout << " # what: " << e.what() << endl;
        return false;
    }
}

bool ok (bool b, const string& name) {
    if (b) return pass(name);
    else return fail(name);
}

bool ok (const std::function<bool()>& f, const string& name) {
    return catch_fail([&]{ return ok(f(), name); }, name);
}

template <class A, class B>
bool is (A a, B b, const string& name) {
    if (!ok(a == b, name)) {
        cout << " # Expected: " << b << endl;
        cout << " # Got:      " << a << endl;
        return false;
    }
    return true;
}

void t (const string& s) {
    catch_fail([&]{
        auto v = parse(s);
        auto s2 = stringify(v);
        return is(s2, s, s);
    }, s);
}

#include <memory>

int main () {
    t("null");
    t("true");
    t("false");
    t("0");
    t("12343");
    t("35.324");
    t("-45.5");
    t("1.3241e-54");
    t("\"foo\"");
    t("\"a \\n b \\r c \\t c \\\\ \\\" \"");
    t("[]");
    t("[2,4,6,null,true,\"asdf\"]");
    t("{}");
    t("{\"asdf\":\"ghjk\"}");
    t("{\"foo\":[4,5,6],\"agfd\":[[[[4]]],{},{\"gtre\":null}],\"goo\":{}}");
    return 0;
}
