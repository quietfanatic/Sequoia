#pragma once

#include <vector>

#include "../util/types.h"
#include "nursery.h"
#include "profile.h"

namespace win32app {

struct App {
    Profile profile;
    Settings settings;
    Nursery nursery;

    App (Profile&&);
    ~App ();

     // Make windows for open trees, open window for urls, and if none
     // of those happens, makes a default window.
    void start (const std::vector<String>& urls);

     // Runs a standard win32 message loop until quit() is called.
    int run ();
    void quit (int code = 0);

     // TEMP
    static App& get ();
};

} // namespace win32app
