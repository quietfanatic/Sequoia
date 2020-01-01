#pragma once

#include <functional>
#include <map>
#include <string>

#include "../util/assert.h"

std::map<std::string, std::function<void()>>& all_tests ();

struct Test {
    Test(const std::string& name, const std::function<void()>& f) {
        all_tests().emplace(name, f);
    }
};

static inline void run_all_tests () {
    for (auto& p : all_tests()) {
        p.second();
    }
}
static inline void run_test (const std::string& name) {
    auto iter = all_tests().find(name);
    A(iter != all_tests().end());
    iter->second();
}
