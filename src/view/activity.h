#pragma once

#include <string>
#include <wil/com.h>
#include <windows.h>

#include "../model/page.h"
#include "../util/types.h"

struct Window;
namespace json { struct Value; }

struct Activity {
    model::PageID page;

    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    HWND webview_hwnd = nullptr;

     // Store this so that we don't navigate from our own SourceChanged handler,
     //  and also to work around special URLs not surviving a round-trip
     //  through navigation
    std::string current_url;

    Activity (model::PageID);
    void update (model::PageID);

    bool navigate_url (const std::string& url);
    void navigate_search (const std::string& search);
    void navigate (const std::string& address);

    void message_to_page (const json::Value& message);

    void resize (RECT available);
    bool is_fullscreen ();
    void leave_fullscreen ();

    ~Activity();
};

Activity* activity_for_page (model::PageID id);
