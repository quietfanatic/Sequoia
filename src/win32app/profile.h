#pragma once

#include "../util/types.h"

extern String profile_name;
extern bool profile_folder_specified;
extern String profile_folder;

namespace settings {

extern String theme;

}

void load_profile ();
void load_settings ();

void save_settings ();

void register_as_browser ();
