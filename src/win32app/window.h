#pragma once

#include <functional>
#include <optional>
#include <windows.h>

#include "../model/model.h"

namespace win32app {
struct App;

struct Window {
    App& app;
    model::ViewID view;
    Window (App&, model::ViewID);
    ~Window();

     ///// Window methods
     // Not sure this belongs here but whatever
    std::function<void()> get_key_handler (
        uint key, bool shift, bool ctrl, bool alt
    );

     ///// OS window stuff
    HWND hwnd;
     // nullopt if not currently fullscreen
    struct Fullscreen {
        WINDOWPLACEMENT old_placement;
    };
    std::optional<Fullscreen> fullscreen;
     // In DIPs (scaled by dpi).  TODO: store these in model
    double sidebar_width = 240;
    double toolbar_height = 28;
    double main_menu_width = 0;
    void reflow ();

     ///// Updating
    model::NodeID current_focused_node;
    void view_updated ();
    void node_updated ();
};

} // namespace win32app
