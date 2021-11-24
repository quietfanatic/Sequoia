#pragma once

#include "../util/types.h"

 // TODO: move to model
struct Settings {
    String theme;
};

struct Profile {
    String name;
    bool folder_specified;
    String folder;

    Profile (Str name = ""sv, Str folder = ""sv);

    Settings load_settings ();
    String db_path ();
    void register_as_browser ();
};

