#pragma once

#include <string>
#include <wil/com.h>
#include <windows.h>

#include "util/types.h"

struct Window;
namespace json { struct Value; }

struct Activity {
    int64 tab;
    Window* window = nullptr;

    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;
    bool can_go_back = false;
    bool can_go_forward = false;
    bool currently_loading = false;

    int64 last_created_new_child = 0;

    Activity(int64 tab);

    void resize (RECT available);
    bool navigate_url (const std::string& url);
    void navigate_search (const std::string& search);
    void navigate_url_or_search (const std::string& address);

    void claimed_by_window (Window*);
    void message_from_webview (json::Value&& message);

    ~Activity();
};

Activity* activity_for_tab (int64 id);
Activity* ensure_activity_for_tab (int64 id);
