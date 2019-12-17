#pragma once

#include "_windows.h"
#include <map>
#include <string>
#include <wil/com.h>

#include "stuff.h"

struct Window;
namespace json { struct Value; }

struct Activity {
    int64 tab;
    Window* window = nullptr;

    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;
    bool can_go_back = false;
    bool can_go_forward = false;

    Activity(int64 tab);

    void claimed_by_window (Window*);
    void resize (RECT available);

    void message_from_webview (json::Value&& message);

    ~Activity();
};

Activity* activity_for_tab (int64 id);
Activity* ensure_activity_for_tab (int64 id);
