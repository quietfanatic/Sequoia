 // This implements a class of infinite-precision fractions that can be compared with
 //   memcmp, so that they can serve as an ordered SQLite BLOB.
 //
 // A Bifractor can constructed as 0, 1, or the bisection of two other Bifractors.
 //   The bisecting constructor can take a bias within 0x01..0xff, with 0x80 being the
 //   halfway point. Lower biases will make the result closer to the left side, so that
 //   if you expect to bisect in one direction contiually, the byte string doesn't get
 //   quite as long.
 // It is an error to try to bisect equal Bifractors.

#pragma once

#include <new>
#include <ostream>

#include "types.h"

struct Bifractor {
    size_t size;
    union {
        uint8* ptr;
        uint8 imm [sizeof(ptr)];
    };

    const uint8* bytes () const {
        if (size <= sizeof(ptr)) return imm;
        else return ptr;
    }
    uint8* bytes () {
        if (size <= sizeof(ptr)) return imm;
        else return ptr;
    }

    String hex () const;

     // Manufacturing
    Bifractor (bool one = false) : size(1) {
        imm[0] = one ? 0xff : 0x00;
    }
    Bifractor (const Bifractor& a, const Bifractor& b, uint bias = 0x80);

     // Martialling
    Bifractor (const void* bytes_ptr, size_t bytes_size);
    Bifractor (const Bifractor& b);
    Bifractor (Bifractor&& b) : size(b.size) {
        ptr = b.ptr;
        b.ptr = nullptr;
        b.size = 0;
    }
    ~Bifractor () {
        if (size > sizeof(ptr)) {
            delete[] ptr;
        }
    }
    Bifractor& operator = (const Bifractor& b) {
        this->~Bifractor();
        new ((void*)this) Bifractor(b);
        return *this;
    }
    Bifractor& operator = (Bifractor&& b) {
        this->~Bifractor();
        new ((void*)this) Bifractor(b);
        return *this;
    }

     // Misc
    static int compare (const Bifractor& a, const Bifractor& b);
};

static inline bool operator < (const Bifractor& a, const Bifractor& b) {
    return Bifractor::compare(a, b) < 0;
}
static inline bool operator <= (const Bifractor& a, const Bifractor& b) {
    return Bifractor::compare(a, b) <= 0;
}
static inline bool operator == (const Bifractor& a, const Bifractor& b) {
    if (a.size != b.size) return false;
    return Bifractor::compare(a, b) == 0;
}
static inline bool operator >= (const Bifractor& a, const Bifractor& b) {
    return Bifractor::compare(a, b) >= 0;
}
static inline bool operator > (const Bifractor& a, const Bifractor& b) {
    return Bifractor::compare(a, b) > 0;
}

static inline std::ostream& operator << (std::ostream& o, const Bifractor& b) {
    o << b.hex();
    return o;
}

