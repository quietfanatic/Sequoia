#include "tests.h"

std::map<std::string, std::function<void()>>& all_tests () {
    static std::map<std::string, std::function<void()>> r;
    return r;
}

