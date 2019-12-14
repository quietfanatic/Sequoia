#pragma once
#include "stuff.h"

#include <iostream>

std::ostream& logstream ();

template <class... Args>
void LOG (Args&&... args) {
    (logstream() << ... << std::forward<Args>(args)) << std::endl;
}
