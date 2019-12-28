#pragma once

#include <string>

#include "stuff.h"

namespace settings {

extern std::string theme;

}

void load_settings ();
void save_settings ();
