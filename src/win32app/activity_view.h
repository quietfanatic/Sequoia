#pragma once

#include <wil/com.h>

#include "../model/model.h"
#include "../util/types.h"
#include "../util/weak.h"

struct ICoreWebView2;
struct ICoreWebView2Controller;
namespace json { struct Value; }

namespace win32app {
struct App;

struct ActivityView : WeakPointable {
    App& app;
    model::ActivityID id;

    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2> webview;
    HWND webview_hwnd = nullptr;

     // Store this so that we don't navigate from our own SourceChanged handler,
     // and also to work around special URLs not surviving a round-trip through
     // navigation.
    String current_url;
     // If activity's loading_at is newer, start a new load
    double current_loading_at;

    ActivityView (App&, model::ActivityID);
    void update ();

     // TODO: make these internal
    void message_to_webview (const json::Value& message);
    void message_from_webview (const json::Value& message);

    bool is_fullscreen ();
    void leave_fullscreen ();

     // For testing
    void wait_for_ready ();
    bool waiting_for_ready = false;

    ~ActivityView();
};

} // namespace win32app
