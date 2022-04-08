#pragma once

#include <wil/com.h>
#include <WebView2.h>

#include "../model/model.h"
#include "tabs.h"

namespace json { struct Value; }

namespace win32app {
struct App;

struct Bark {
    App& app;
    model::TreeID tree;
    Bark (App&, model::TreeID);
    ~Bark ();

     ///// Bark methods
    void select_location ();

     ///// Interaction with webview
    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    HWND webview_hwnd = nullptr;
    bool ready = false;
     // TODO: make these private
    void message_to_webview (const json::Value& message);
    void message_from_webview (const json::Value& message);

     ///// Updating
    TabTree current_tabs;
    void update (const model::Update&);

     // Mainly for testing
    void wait_for_ready ();
    bool waiting_for_ready = false;
};

} // namespace win32app
