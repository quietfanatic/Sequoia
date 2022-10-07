#pragma once

#include <memory>
#include <vector>

#include "../model/data.h"
#include "../util/types.h"
#include "nursery.h"
#include "profile.h"

namespace win32app {
struct Bark;

struct App : Observer {
    Profile profile;
    Settings settings;
    Nursery nursery;

     // Makes all windows invisible, intended for testing.
    bool headless = false;

    std::unordered_map<int64, std::unique_ptr<Bark>> barks;

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

    void Observer_after_commit (
        const std::vector<int64>& updated_tabs,
        const std::vector<int64>& updated_windows
    );
};

} // namespace win32app
