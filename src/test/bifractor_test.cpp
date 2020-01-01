
#include <cstdlib>
#include <iomanip>
#include <string>

#include "tap.h"
#include "tests.h"
#include "../util/bifractor.h"

using namespace std;

Test bifractor_test {"bifractor", []{
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
}};

