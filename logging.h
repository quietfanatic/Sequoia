#pragma once
#include "stuff.h"

#include <ostream>

std::wostream& logstream ();

namespace {

template <class... Args>
std::wostream& log_args (std::wostream& log, Args&&... args);

template <>
std::wostream& log_args (std::wostream& log) {
    return log;
}

template <class Arg>
std::wostream& log_args (std::wostream& log, Arg&& arg) {
    log << std::forward<Arg>(arg);
    return log;
}

template <class Arg1, class Arg2, class... Args>
std::wostream& log_args (std::wostream& log, Arg1&& arg1, Arg2&& arg2, Args&&... args) {
    log << std::forward<Arg1>(arg1) << ", ";
    log_args(log, std::forward<Arg2>(arg2), std::forward<Args>(args)...);
    return log;
}

template <class... Args>
void LOG (Args&&... args) {
    log_args(logstream(), std::forward<Args>(args)...) << std::endl << std::flush;
}

}
