#include "bifractor.h"

#include <memory>
#include <stdexcept>

#include "text.h"

std::string Bifractor::hex () const {
    std::string r (size*2, 0);
    for (size_t i = 0; i < size; i++) {
        r[i*2] = to_hex_digit(bytes()[i] >> 4);
        r[i*2+1] = to_hex_digit(bytes()[i] & 0xf);
    }
    return r;
}

Bifractor::Bifractor (const Bifractor& a, const Bifractor& b, uint bias) {
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
    throw std::logic_error("Tried to bisect two Bifractors that were equal.");
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
