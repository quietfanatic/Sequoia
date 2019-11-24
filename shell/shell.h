#pragma once

#include <wil/com.h>

#include "../_windows.h"
#include "../stuff.h"

namespace json { struct Value; }

struct Tab;
struct Window;

struct Shell {
    Window* window;
    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;
    Shell (Window*);

    void message_from_shell (json::Value&& message);
    void message_to_shell (json::Value&& message);

    void update_tabs (Tab** tabs, size_t length);
    void update_tab (Tab* t) { update_tabs(&t, 1); }

    RECT resize (RECT available);

    WebView* active_webview ();
};
