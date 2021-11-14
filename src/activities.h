#pragma once

#include <string>
#include <wil/com.h>
#include <windows.h>

#include "model/data.h"
#include "util/types.h"

struct Window;
namespace json { struct Value; }

struct Activity {
    model::Page page;
    Window* window = nullptr;

    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    HWND webview_hwnd = nullptr;
    bool can_go_back = false;
    bool can_go_forward = false;
    bool currently_loading = false;

     // Workaround for special URLs not surviving a round-trip the navigation
    std::string navigated_url;

    Activity (const model::Page&);

    void resize (RECT available);
    bool navigate_url (const std::string& url);
    void navigate_search (const std::string& search);
    void navigate_url_or_search (const std::string& address);

    void claimed_by_window (Window*);
    void message_from_webview (json::Value&& message);
    void message_to_webview (json::Value&& message);

    bool is_fullscreen ();
    void leave_fullscreen ();

    ~Activity();
};

Activity* activity_for_page (model::PageID id);
Activity* ensure_activity_for_page (model::PageID id);
