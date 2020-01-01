 // This implements a class of infinite-precision fractions that can be compared with
 //   memcmp, so that they can serve as an ordered SQLite BLOB.
 //
 // A Bifractor can constructed as 0, 1, or a point between two other Bifractors.
 //
 // Technically speaking, they are represented as a base-255 string (0x00..0xfe), where the
 //   byte 0xff represents infinitely-repeating 255s.  This is because for memcmp,
 //   "\xff\x00" compares larger than "\xff", so we have to avoid that situation.  Also,
 //   bisecting a Bifractor doesn't necessarily take the strict mathematical halfway point;
 //   instead it'll pick a number with as few bytes as possible to save space.

#pragma once

#include <new>

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

     // Manufacturing
    Bifractor (bool one = false) : size(1) {
        imm[0] = one ? 0xff : 0x00;
    }
    Bifractor (const Bifractor& a, const Bifractor& b);

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

