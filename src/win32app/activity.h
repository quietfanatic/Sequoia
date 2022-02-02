#pragma once

#include <wil/com.h>
#include <WebView2.h>
#include <windows.h>

#include "../model/model.h"
#include "../util/types.h"

namespace json { struct Value; }

namespace win32app {
struct App;

struct Activity {
    App& app;
    model::NodeID node;

    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    HWND webview_hwnd = nullptr;

     // Store this so that we don't navigate from our own SourceChanged handler,
     // and also to work around special URLs not surviving a round-trip through
     // navigation.
    String current_url;

    Activity (App&, model::NodeID);
    void node_updated ();

    bool navigate_url (Str url);
    void navigate_search (Str search);
    void navigate (Str address);

    void message_to_webview (const json::Value& message);
    void message_from_webview (const json::Value& message);

    bool is_fullscreen ();
    void leave_fullscreen ();

    ~Activity();
};

} // namespace win32app
