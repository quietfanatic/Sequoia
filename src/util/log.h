#pragma once

#include <ostream>

#include "types.h"

extern std::ostream* logstream;

void init_log (const std::string& filename);

uint64 logging_timestamp ();

namespace {

template <class... Args>
std::ostream& log_args (std::ostream& log, Args&&... args);

template <>
std::ostream& log_args (std::ostream& log) {
    return log;
}

template <class Arg>
std::ostream& log_args (std::ostream& log, Arg&& arg) {
    log << std::forward<Arg>(arg);
    return log;
}

template <class Arg1, class Arg2, class... Args>
std::ostream& log_args (std::ostream& log, Arg1&& arg1, Arg2&& arg2, Args&&... args) {
    log << std::forward<Arg1>(arg1) << ", ";
    log_args(log, std::forward<Arg2>(arg2), std::forward<Args>(args)...);
    return log;
}

template <class... Args>
void LOG (Args&&... args) {
    log_args(*logstream, logging_timestamp(), std::forward<Args>(args)...) << std::endl << std::flush;
}

}
