#pragma once

#include "types.h"

template <class T>
constexpr uint64 x31_hash (T* s) {
    uint64 h = 0;
    for (; *s != 0; s++) {
        h = (h << 5) - h + *s;
    }
    return h;
}

 // For std::string-like objects
template <class T>
constexpr uint64 x31_hash (T s) {
    uint64 h = 0;
    for (auto c : s) {
        h = (h << 5) - h + c;
    }
    return h;
}
