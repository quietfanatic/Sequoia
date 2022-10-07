#pragma once

#include <vector>

#include "../util/types.h"
#include "settings.h"

namespace win32app {

struct App {
    App ();
    ~App ();

     // Make windows for open trees, open window for urls, and if none
     // of those happens, makes a default window.
    void start (const std::vector<String>& urls);

     // Runs a standard win32 message loop until quit() is called.
    int run ();
    void quit (int code = 0);
};

} // namespace win32app
