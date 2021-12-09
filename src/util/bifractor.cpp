#include "bifractor.h"

#include <memory>
#include <stdexcept>

#include "error.h"
#include "text.h"

static void validate (const Bifractor& b) {
    if (b.size >= 2) {
        if (b.bytes()[0] == 0xff) {
            ERR("First byte in Bifractor bytestring cannot be 0xFF (unless it's the only byte)"sv);
        }
        if (b.bytes()[b.size-1] == 0x00) {
            ERR("Last byte in Bifractor bytestring cannot be 0x00 (unless it's the only byte)"sv);
        }
    }
}

std::string Bifractor::hex () const {
    AA(size);
    std::string r (size*2, 0);
    for (size_t i = 0; i < size; i++) {
        r[i*2] = to_hex_digit(bytes()[i] >> 4);
        r[i*2+1] = to_hex_digit(bytes()[i] & 0xf);
    }
    return r;
}

Bifractor::Bifractor (const Bifractor& a, const Bifractor& b, float bias) :
    Bifractor()
{
    AA(a.size);
    AA(b.size);
    AA(bias >= 0 && bias <= 1);
    size_t max_size = (a.size > b.size ? a.size : b.size) + 1;

    uint8 stack_buf [128];
    std::unique_ptr<uint8[]> heap_buf (
        max_size <= 128 ? nullptr : new uint8[max_size]
    );
    uint8* buf = max_size <= 128 ? stack_buf : heap_buf.get();

    uint carry_b = 0;
    for (size_t i = 0; i < max_size; i++) {
         // Extend a and b infinitely to the right with 0x00
        uint av = (i < a.size ? a.bytes()[i] : 0);
        uint bv = (i < b.size ? b.bytes()[i] : 0) + carry_b;
        if (av > bv) {
            ERR("Tried to bisect two Bifractors that were in the wrong order."sv);
        }
        else if (av == bv) {
            buf[i] = av;
        }
        else if (av + 1 == bv) {
            buf[i] = av;
            carry_b = 0x100;
        }
        else {
             // Lerp between the lower and upper bound (then round)
            uint middle = uint(av * (1 - bias) + bv * bias + 0.5);
             // Make sure we don't accidentally equal either side.  Lerping
             // between av+1 and bv-1 instead would save us from having to do
             // this adjustment, but it would make the bias less effective.
            if (middle == av) middle += 1;
            if (middle == bv) middle -= 1;
            buf[i] = middle;

            const_cast<size_t&>(size) = i+1;
            if (size > sizeof(ptr)) {
                const_cast<const uint8*&>(ptr) = new uint8 [size];
            }
            for (size_t i = 0; i < size; i++) {
                const_cast<uint8*>(bytes())[i] = buf[i];
            }
#ifndef NDEBUG
            try { validate(*this); }
            catch (...) { this->~Bifractor(); throw; }
#endif
            return;
        }
    }
    ERR("Tried to bisect two Bifractors that were equal."sv);
}

Bifractor::Bifractor (const uint8* bytes_ptr, size_t bytes_size) :
    size(bytes_size)
{
    if (size > sizeof(ptr)) {
        const_cast<const uint8*&>(ptr) = new uint8 [size];
    }
    for (size_t i = 0; i < size; i++) {
        const_cast<uint8*>(bytes())[i] = bytes_ptr[i];
    }
    try { validate(*this); }
    catch (...) { this->~Bifractor(); throw; }
}

int operator <=> (const Bifractor& a, const Bifractor& b) {
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
        Bifractor b {1};
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {zero, b};
            if (new_b <= zero || new_b >= one || new_b >= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting downwards");
        is(b.size, 1111 / 8 + 1, "Unbiased bifracting results in iterations/8 length string");
    }
    {
        Bifractor b {1};
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {zero, b, 31/32.0};
            if (new_b <= zero || new_b >= one || new_b >= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting downwards, biased");
         // Why is this +4 and not +1?  Because only the last byte in the
         // bytestring is lerped, rather than the entire remaining interval,
         // making biases less effective when the last bytes are close together.
         // In the extreme case where the last bytes are 2 apart, the bias is
         // completely meaningless, and the last byte is set to the only value
         // left between them.
        is(b.size, 1111 / 128 + 4, "Biased bifracting results in smaller string");
    }
    {
        Bifractor b {1};
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {zero, b, 1/32.0};
            if (new_b <= zero || new_b >= one || new_b >= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting downwards, wrongly biased");
        is(b.size, 1111 / 2 + 1, "Wrongly biased bifracting results in larger string");
    }
    {
        Bifractor b {0};
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {b, one};
            if (new_b <= zero || new_b >= one || new_b <= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting upwards");
        is(b.size, 1111 / 8 + 1, "Unbiased bifracting results in iterations/8 length string");
    }
    {
        Bifractor b {0};
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {b, one, 1/32.0};
            if (new_b <= zero || new_b >= one || new_b <= b) {
                okay = false;
                break;
            }
            b = new_b;
        }
        ok(okay, "Continually bifracting upwards, biased");
         // Why +4 and not +1?  See the explanation above.
        is(b.size, 1111 / 128 + 4, "Biased bifracting results in smaller string");
    }
    {
        Bifractor b {0};
        bool okay = true;
        for (size_t i = 0; i < 1111; i++) {
            Bifractor new_b {b, one, 31/32.0};
            if (new_b <= zero || new_b >= one || new_b <= b) {
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
                Bifractor new_b {zero, b};
                if (new_b <= zero || new_b >= one || new_b >= b) {
                    okay = false;
                    break;
                }
                b = new_b;
            }
            else {
                Bifractor new_b {b, one};
                if (new_b <= zero || new_b >= one || new_b <= b) {
                    okay = false;
                    break;
                }
                b = new_b;
            }
        }
        ok(okay, "Continually bifracting randomly between 0 and 1");
         // Technically this could randomly fail, but it is extremely unlikely.
         // (something like 1/2^(6*8), or less than one in several pentillion)
        ok(b.size < 6, "Bifracting randomly between 0 and 1 doesn't make a very large string");
    }
    {
        Bifractor low_b {0};
        Bifractor high_b {1};
        bool okay = true;
        Bifractor b;
        for (size_t i = 0; i < 1111; i++) {
            b = Bifractor{low_b, high_b};
            if (b <= zero || b >= one || b <= low_b || b >= high_b) {
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
        ok(okay, "Continually bifracting randomly narrowing");
        is(b.size, 1111 / 8 + 1, "Randomly narrowing bifracting makes a longish string");
    }
    is(
        Bifractor{Bifractor{Bifractor{0}, Bifractor{1}}, Bifractor{1}}.hex(),
        "C0",
        "Brief test of hex()"
    );
});

#endif
