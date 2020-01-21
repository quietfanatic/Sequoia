#pragma once

#include <string>

extern std::string profile_name;
extern bool profile_folder_specified;
extern std::string profile_folder;

namespace settings {

extern std::string theme;

}

void load_profile ();
void load_settings ();

void save_settings ();
