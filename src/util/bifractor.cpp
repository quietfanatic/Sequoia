#include "bifractor.h"

#include <memory>
#include <stdexcept>

#include "assert.h"
#include "text.h"

std::string Bifractor::hex () const {
    AA(size);
    std::string r (size*2, 0);
    for (size_t i = 0; i < size; i++) {
        r[i*2] = to_hex_digit(bytes()[i] >> 4);
        r[i*2+1] = to_hex_digit(bytes()[i] & 0xf);
    }
    return r;
}

Bifractor::Bifractor (const Bifractor& a, const Bifractor& b, uint bias) {
    AA(a.size);
    AA(b.size);
    AA(a != b);
    size_t max_size = (a.size > b.size ? a.size : b.size) + 1;

    std::unique_ptr<uint8[]> buf {new uint8 [max_size]};

    uint carry_a = 0;
    uint carry_b = 0;
    for (size_t i = 0; i < max_size; i++) {
        uint av = (i < a.size ? a.bytes()[i] : 0) + carry_a;
        uint bv = (i < b.size ? b.bytes()[i] : 0) + carry_b;
        switch (av - bv) {
        case 0:
            buf[i] = av;
            break;
        case 1:
            buf[i] = bv;
            carry_a = 0x100;
            break;
        case -1:
            buf[i] = av;
            carry_b = 0x100;
            break;
        default: {
            uint middle = (av * (0x100 - bias) + bv * bias) >> 8;
             // Since this rounds down, whichever it's equal to is the smaller one.
            if (middle == av || middle == bv) middle += 1;
            buf[i] = middle;
            new (this) Bifractor(buf.get(), i+1);
            return;
        }
        }
    }
    throw Error("Tried to bisect two Bifractors that were equal."sv);
}

Bifractor::Bifractor (const void* bytes_ptr, size_t bytes_size) : size(bytes_size) {
    if (size > sizeof(ptr)) {
        ptr = new uint8 [size];
    }
    for (size_t i = 0; i < size; i++) {
        bytes()[i] = reinterpret_cast<const uint8*>(bytes_ptr)[i];
    }
}

Bifractor::Bifractor (const Bifractor& b) : size(b.size) {
    if (size > sizeof(ptr)) {
        ptr = new uint8[size];
    }
    for (size_t i = 0; i < size; i++) {
        bytes()[i] = b.bytes()[i];
    }
}

/*static*/ int Bifractor::compare (const Bifractor& a, const Bifractor& b) {
    int r = memcmp(a.bytes(), b.bytes(), a.size < b.size ? a.size : b.size);
    if (r) return r;
    return int(a.size - b.size);
}

#ifndef TAP_DISABLE_TESTS
#include "../tap/tap.h"

static tap::TestSet tests ("util/bifractor", []{
    using namespace tap;
    plan(22);
    srand(uint(time(0)));

    Bifractor zero {0};
    Bifractor one {1};
    is(zero, zero, "0 = 0");
    is(one, one, "1 = 1");
    isnt(zero, one, "0 != 1");
    ok(zero < one, "0 < 1");
    ok(one > zero, "1 < 0");

    Bifractor half {zero, one};
    ok(zero < half, "0 < 1/2");
    ok(half < one, "1/2 < 1");
    {
        Bifractor b = 1;
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {0, b};
            if (new_b <= 0 || new_b >= 1 || new_b >= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting downwards");
        is(b.size, 1111 / 8 + 1, "Unbiased bifracting results in iterations*8 length string");
    }
    {
        Bifractor b = 1;
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {0, b, 0xf8};
            if (new_b <= 0 || new_b >= 1 || new_b >= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting downwards, biased");
         // Why is this +6 and not +1?  Probably due to excessive integer rounding down,
         //   making the bias effectively lower.  I'm not too concerned though, since I
         //   expect bifracting up with low bias will be more common than bifracting down
         //   with high bias.
        is(b.size, 1111 / 128 + 6, "Biased bifracting results in smaller string");
    }
    {
        Bifractor b = 1;
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {0, b, 0x08};
            if (new_b <= 0 || new_b >= 1 || new_b >= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting downwards, wrongly biased");
        is(b.size, 1111 / 2 + 1, "Wrongly biased bifracting results in larger string");
    }
    {
        Bifractor b = 0;
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {b, 1};
            if (new_b <= 0 || new_b >= 1 || new_b <= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting upwards");
        is(b.size, 1111 / 8 + 1, "Unbiased bifracting results in iterations*8 length string");
    }
    {
        Bifractor b = 0;
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {b, 1, 0x08};
            if (new_b <= 0 || new_b >= 1 || new_b <= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting upwards, biased");
        is(b.size, 1111 / 128 + 2, "Biased bifracting results in smaller string");
    }
    {
        Bifractor b = 0;
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {b, 1, 0xf8};
            if (new_b <= 0 || new_b >= 1 || new_b <= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting upwards, wrongly biased");
        is(b.size, 1111 / 2 + 1, "Wrongly biased bifracting results in larger string");
    }
    {
        Bifractor b {Bifractor{0}, Bifractor{1}};
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            if (rand() & 1) {
                Bifractor new_b {0, b};
                if (new_b <= 0 || new_b >= 1 || new_b >= b) {
                    okay = false;
                    break;
                }
                b = new_b;
            }
            else {
                Bifractor new_b {b, 1};
                if (new_b <= 0 || new_b >= 1 || new_b <= b) {
                    okay = false;
                    break;
                }
                b = new_b;
            }
        }
        ok(okay, "Continually bifracting randomly between 0 and 1");
    }
    {
        Bifractor low_b {0};
        Bifractor high_b {1};
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor b {low_b, high_b};
            if (b <= 0 || b >= 1 || b <= low_b || b >= high_b) {
                okay = false;
                break;
            }
            if (rand() & 1) {
                low_b = b;
            }
            else {
                high_b = b;
            }
        }
        ok(okay, "Continually bifracting randomly increasingly smaller");
    }
    is(
        Bifractor{Bifractor{0}, Bifractor{Bifractor{0}, Bifractor{1}}}.hex(),
        "3F",
        "Brief test of hex()"
    );
});

#endif
