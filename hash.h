#pragma once

#include "util.h"

template <class T>
constexpr uint64_t x31_hash (const T* s) {
    uint64_t h = 0;
    for (; *s != 0; s++) {
        h = (h << 5) - h + *s;
    }
    return h;
}
