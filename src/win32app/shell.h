#pragma once

#include <wil/com.h>
#include <WebView2.h>

#include "../model/model.h"
#include "../bark/tabs.h"

namespace json { struct Value; }

namespace win32app {
struct App;

struct Shell {
    App& app;
    model::ViewID view;
    Shell (App&, model::ViewID);
    ~Shell ();

     ///// Shell methods
    void select_location ();

     ///// Interaction with webview
    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    HWND webview_hwnd = nullptr;
    bool ready = false;
     // TODO: make these internal
    void message_to_webview (const json::Value& message);
    void message_from_webview (const json::Value& message);

     ///// Updating
    bark::TabTree current_tabs;
    void update (const model::Update&);

     // Mainly for testing
    void wait_for_ready ();
    bool waiting_for_ready = false;
};

} // namespace win32app
