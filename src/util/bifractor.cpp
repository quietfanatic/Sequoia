#include "bifractor.h"

#include <memory>
#include <stdexcept>

Bifractor::Bifractor (const Bifractor& a, const Bifractor& b) {
    size_t max_size = (a.size > b.size ? a.size : b.size) + 2;

    std::unique_ptr<uint8[]> buf {new uint8 [max_size]};

    uint carry_a = 0;
    uint carry_b = 0;
    size_t i;
    for (i = 0; i < max_size; i++) {
        uint av = (i < a.size ? a.bytes()[i] : 0) + carry_a;
        uint bv = (i < b.size ? b.bytes()[i] : 0) + carry_b;
        if (av == bv) {
            buf[i] = av;
        }
        else {
            uint middle = (av + bv) / 2;
             // Since this rounds down, whichever it's equal to is the smaller one.
            if (middle == av) {
                buf[i] = av;
                carry_b = 0x100;
            }
            else if (middle == bv) {
                buf[i] = bv;
                carry_a = 0x100;
            }
            else {
                 // We found a satisfactory middle.
                buf[i++] = middle;
                break;
            }
        }
    }
    if (i == max_size) {
        throw std::logic_error("Tried to bisect two Bifractors that were equal.");
    }
    new (this) Bifractor(buf.get(), i);
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
