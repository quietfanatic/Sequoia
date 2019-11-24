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

    void interpret_web_message (const json::Value& message);

    void add_tab (Tab* tab);
    void update_tab (Tab* tab);
    RECT resize (RECT available);

    WebView* active_webview ();
};
