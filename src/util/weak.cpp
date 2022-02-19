#include "weak.h"

#include <unordered_map>

#include "error.h"

struct WeakRegistry {
    size_t next_id = 1;
    std::unordered_map<uint64, WeakPointable*> map;
    static WeakRegistry& get () {
        static WeakRegistry r;
        return r;
    }
    static size_t enregister (WeakPointable* p) {
        auto& reg = get();
        auto [iter, emplaced] = reg.map.emplace(reg.next_id, p);
        AA(emplaced);
        return reg.next_id++;
    }
    static void deregister (size_t id) {
        auto& reg = get();
        reg.map.erase(id);
    }
    static WeakPointable* find (size_t id) {
        auto& reg = get();
        auto iter = reg.map.find(id);
        if (iter == reg.map.end()) return nullptr;
        else return iter->second;
    }
};

WeakPointable::WeakPointable () : weak_id(WeakRegistry::enregister(this)) { }
WeakPointable::~WeakPointable () { WeakRegistry::deregister(weak_id); }
WeakPointable* WeakPointable::get_strong_ptr (size_t id) {
    return WeakRegistry::find(id);
}

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

struct TestPointable : WeakPointable {
    int foo;
    TestPointable (int f) : foo(f) { }
};

tap::TestSet tests ("util/weak", []{
    using namespace tap;

    TestPointable* foo;

    doesnt_throw([&]{
        foo = new TestPointable(4);
    }, "new WeakPointable");

    WeakPtr<TestPointable> ptr;
    ok(!ptr, "default constructed WeakPtr is false");
    ptr = foo;
    ok(ptr, "WeakPtr constructed from ponter is true");
    is(ptr->foo, 4, "operator ->");
    doesnt_throw([&]{
        delete foo;
    }, "delete WeakPointable");
    ok(!ptr, "WeakPtr becomes false after pointed object is deleted");

    done_testing();
});

#endif
