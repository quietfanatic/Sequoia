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

#ifndef TAP_DISABLE_TESTS
struct ProfileTestEnvironment {
    String test_folder;
    String profile_name;
    String profile_folder;
    ProfileTestEnvironment();
    ~ProfileTestEnvironment();
};
#endif

} // namespace win32app
