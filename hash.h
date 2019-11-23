#pragma once

#include "stuff.h"

template <class T>
constexpr uint64 x31_hash (const T* s) {
    uint64 h = 0;
    for (; *s != 0; s++) {
        h = (h << 5) - h + *s;
    }
    return h;
}
