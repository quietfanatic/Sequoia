#pragma once

#include <wil/com.h>
#include <WebView2.h>

#include "../model/tabs.h"
#include "../model/view.h"
#include "../model/transaction.h"

namespace model { struct Update; }
namespace json { struct Value; }
struct App;

struct Shell {
    App* app;
    model::ViewID view;
    model::TabTree tabs;

    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    HWND webview_hwnd = nullptr;
    bool ready = false;

    Shell (App*, model::ViewID);

    void select_location ();

    void message_to_webview (const json::Value& message);
    void message_from_webview (const json::Value& message);

    void update (const model::Update&);
    ~Shell ();
};

Shell* shell_for_view (model::ViewID);
