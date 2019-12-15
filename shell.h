#pragma once

#include <wil/com.h>

#include "_windows.h"
#include "stuff.h"
#include "tabs.h"

namespace json { struct Value; }

struct Tab;
struct Window;

struct Shell : TabObserver {
    Window* window;
    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;
    Shell (Window*);

    RECT resize (RECT available);

    void TabObserver_on_commit (const std::vector<Tab*>& updated_tabs) override;

    void message_from_shell (json::Value&& message);
    void message_to_shell (json::Value&& message);

    WebView* active_webview ();
};
