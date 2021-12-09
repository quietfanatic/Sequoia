#pragma once

 // This implements a class of infinite-precision fractions that can be compared
 // with memcmp, so that they can serve as an ordered SQLite BLOB.
 //
 // A Bifractor can constructed as 0, 1, or the bisection of two other
 // Bifractors.  The bisecting constructor can take a bias from 0.0 to 1.0.
 // Lower biases will make the result closer to the left side, so that if you
 // expect to bisect in one direction continually, the byte string doesn't get
 // quite as long.  The bias is not guaranteed to be treated exactly.  When
 // bisecting between two Bifractors, the left one must be strictly lesser than
 // the right one.
 //
 // Internally, Bifractor(0) is represented as X'00', and Bifractor(1) as X'FF'.
 // For all other Bifractors, the first byte cannot be 0xff and the last byte
 // cannot be 0x00.  The bytestring is treated as though it extends infinitely
 // to the right with 0x00 bytes.  The empty bytestring is the default value and
 // is invalid for any usage.

#include <new>
#include <ostream>

#include "error.h"
#include "types.h"

struct Bifractor {
    const size_t size;
    union {
        const uint8* const ptr;
        const uint8 imm [sizeof(ptr)];
    };

    const uint8* bytes () const {
        if (size <= sizeof(ptr)) return imm;
        else return ptr;
    }

    String hex () const;

    constexpr Bifractor () : size(0), ptr(nullptr) { }

     // Although this takes a float, you should only use it with 0 or 1.
    explicit constexpr Bifractor (float bias) :
        size(1), imm{uint8(bias * 0xff + 0.5)}
    {
        AA(bias >= 0 && bias <= 1);
    }

     // a must be strictly less than b
    Bifractor (const Bifractor& a, const Bifractor& b, float bias = 0.5);

     // First byte cannot be 0xFF and last byte cannot be 0x00,
     // unless bytes_size is 0.
    Bifractor (const uint8* bytes_ptr, size_t bytes_size);

    Bifractor (const Bifractor& b) : Bifractor(b.bytes(), b.size) { }
    Bifractor (Bifractor&& b) : size(b.size), ptr(b.ptr) {
        const_cast<const uint8*&>(b.ptr) = nullptr;
        const_cast<size_t&>(b.size) = 0;
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
static inline bool operator != (const Bifractor& a, const Bifractor& b) {
    return !(a == b);
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

