#pragma once

#include "_windows.h"
#include <wil/com.h>

#include "data.h"
#include "stuff.h"

namespace json { struct Value; }

struct Window;

struct Shell : Observer {
    Window* window;
    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;
    Shell (Window*);

    RECT resize (RECT available);

    void Observer_after_commit (const std::vector<int64>& updated_tabs) override;

    void message_from_shell (json::Value&& message);
    void message_to_shell (json::Value&& message);

    WebView* active_webview ();
};