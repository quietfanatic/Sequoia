#pragma once

#include <string>

extern std::string profile_name;
extern std::string profile_folder;

namespace settings {

extern std::string theme;

}

void load_settings (char** argv, int argc);
void save_settings ();
