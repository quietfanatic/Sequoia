#pragma once

#include <wil/com.h>
#include <WebView2.h>
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
    String current_url;

    Activity (model::PageID);
    void page_updated ();

    bool navigate_url (Str url);
    void navigate_search (Str search);
    void navigate (Str address);

    void message_to_webview (const json::Value& message);
    void message_from_webview (const json::Value& message);

    bool is_fullscreen ();
    void leave_fullscreen ();

    ~Activity();
};

Activity* activity_for_page (model::PageID id);
