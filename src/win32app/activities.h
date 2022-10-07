#pragma once

#include <string>
#include <wil/com.h>
#include <windows.h>
#include <webview2.h>

#include "../util/types.h"

namespace json { struct Value; }

namespace win32app {
struct Window;

struct Activity {
    int64 tab;
    Window* window = nullptr;

    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    HWND webview_hwnd = nullptr;
    bool can_go_back = false;
    bool can_go_forward = false;
    bool currently_loading = false;

    int64 last_created_new_child = 0;
     // Workaround for special URLs not surviving a round-trip the navigation
    std::string navigated_url;

    Activity(int64 tab);

    void resize (RECT available);
    bool navigate_url (Str url);
    void navigate_search (Str search);
    void navigate_url_or_search (Str address);

    void claimed_by_window (Window*);
    void message_from_webview (json::Value&& message);
    void message_to_webview (json::Value&& message);

    bool is_fullscreen ();
    void leave_fullscreen ();

    ~Activity();
};

Activity* activity_for_tab (int64 id);
Activity* ensure_activity_for_tab (int64 id);

} // namespace win32app
