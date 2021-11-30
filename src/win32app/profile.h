#pragma once

#include "../util/types.h"

namespace win32app {

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

} // namespace win32app
