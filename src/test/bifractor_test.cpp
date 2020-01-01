
#include <cstdlib>
#include <iomanip>
#include <string>

#include "tap.h"
#include "tests.h"
#include "../util/bifractor.h"

using namespace std;

std::ostream& operator << (std::ostream& o, const Bifractor& b) {
    for (size_t i = 0; i < b.size; i++) {
        cout << std::hex << std::setfill('0') << std::setw(2) << uint(b.bytes()[i]);
    }
    return o;
}

Test bifractor_test {"bifractor", []{
    plan(10);
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
        cout << "#" << b << endl;
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
        cout << "#" << b << endl;
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
        ok(okay, "Continually bifracting between 0 and 1");
        cout << "#" << b << endl;
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
        cout << "#" << low_b << endl;
        cout << "#" << high_b << endl;
    }

}};

