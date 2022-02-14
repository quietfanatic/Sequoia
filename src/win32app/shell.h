#pragma once

#include <wil/com.h>
#include <WebView2.h>

#include "../model/model.h"
#include "tabs.h"

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
    void message_to_webview (const json::Value& message);
    void message_from_webview (const json::Value& message);

     ///// Updating
    TabTree current_tabs;
    void update (const model::Update&);
};

} // namespace win32app
