#pragma once

#include <wil/com.h>
#include <windows.h>

#include "data.h"
#include "stuff.h"

namespace json { struct Value; }

struct Window;

struct Shell : Observer {
    Window* window ();
    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;
    Shell ();

     // These are in device pixels, not dips
    uint sidebar_width = 480;
    uint toolbar_height = 56;
    uint main_menu_width = 0;

    RECT resize (RECT available);

    void Observer_after_commit (
        const std::vector<int64>& updated_tabs,
        const std::vector<int64>& updated_windows
    ) override;

    void focus_tab (int64 focused_tab);
    void send_tabs (const std::vector<int64>& updated_tabs);

    void message_from_shell (json::Value&& message);
    void message_to_shell (json::Value&& message);

    WebView* active_webview ();
};
