#pragma once

#include <functional>

#include "../model/view.h"
#include "os_window.h"

struct Window {
    model::ViewID view;

     // For comparison during updates
    model::ViewData current_view_data;

     // TODO: merge this in here
    OSWindow os_window;

     // In DIPs (scaled by dpi).  TODO: store these in model
    double sidebar_width = 240;
    double toolbar_height = 28;
    double main_menu_width = 0;

     // TODO: merge these into wndproc after merging OSWindow in
    void on_hidden ();
    void resize ();
    void close();

    std::function<void()> get_key_handler (uint key, bool shift, bool ctrl, bool alt);

    void view_updated ();
    void page_updated ();

    Window (const model::ViewID&);
    ~Window();
};

Window* window_for_view (model::ViewID);
Window* window_for_page (model::PageID);
