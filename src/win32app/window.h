#pragma once

#include <functional>
#include <windows.h>

#include "../model/view.h"

struct Window {
    model::ViewID view;

     // For comparison during updates
    model::ViewData current_view_data;

    HWND hwnd;
    WINDOWPLACEMENT placement_before_fullscreen;
     // In DIPs (scaled by dpi).  TODO: store these in model
    double sidebar_width = 240;
    double toolbar_height = 28;
    double main_menu_width = 0;

    void reflow ();

    std::function<void()> get_key_handler (uint key, bool shift, bool ctrl, bool alt);

    void view_updated ();
    void page_updated ();

    Window (const model::ViewID&);
    ~Window();
};

Window* window_for_view (model::ViewID);
Window* window_for_page (model::PageID);
