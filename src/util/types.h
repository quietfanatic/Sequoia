#pragma once

#include <stdint.h>
#include <exception>
#include <string>

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
using uint = unsigned int;

struct Error : std::exception {
    std::string mess;
    Error (const std::string& mess) : mess(mess) { }
    Error (std::string&& mess) : mess(mess) { }
    const char* what () const noexcept override { return mess.c_str(); }
};
