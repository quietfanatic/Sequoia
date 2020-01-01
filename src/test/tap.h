#include <functional>
#include <iostream>
#include <string>

namespace {

size_t run = 0;

void plan (size_t count) {
    std::cout << "1.." << count << std::endl;
}

bool pass (const std::string& name) {
    std::cout << "ok " << ++run << "  # " << name << std::endl;
    return true;
}

bool fail (const std::string& name) {
    std::cout << "ok " << ++run << "  # " << name << std::endl;
    return false;
}

bool catch_fail (const std::function<bool()>& f, const std::string& name) {
    try { return f(); }
    catch (std::exception& e) {
        fail(name);
        std::cout << " # Threw exception of type: " << typeid(e).name() << std::endl;
        std::cout << " # what: " << e.what() << std::endl;
        return false;
    }
}

bool ok (bool b, const std::string& name) {
    if (b) return pass(name);
    else return fail(name);
}

bool ok (const std::function<bool()>& f, const std::string& name) {
    return catch_fail([&]{ return ok(f(), name); }, name);
}

template <class A, class B>
bool is (A a, B b, const std::string& name) {
    if (!ok(a == b, name)) {
        std::cout << " # Expected: " << b << std::endl;
        std::cout << " # Got:      " << a << std::endl;
        return false;
    }
    return true;
}

template <class A, class B>
bool isnt (A a, B b, const std::string& name) {
    return ok(a != b, name);
}

}
