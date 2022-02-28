#pragma once

 // This class has two purposes:
 //   - Holds a global message window for IPC
 //   - Makes webviews
 // Eventually these should be split.
 //
 // (A nursery is a place where new trees are grown)

#include <windows.h>
#include <wil/com.h>
#include <WebView2.h>

#include <functional>

#include "../util/types.h"
#include "profile.h"

namespace win32app {
struct App;

 // If the nursery already exists in another process, returns it
HWND existing_nursery (const Profile&);

struct Nursery {
    App& app;
    Nursery (App&);
    ~Nursery ();

    HWND hwnd = nullptr;

    void new_webview (
        const std::function<void(ICoreWebView2Controller*, ICoreWebView2*, HWND)>& then
    );

    String16 edge_udf;
    wil::com_ptr<ICoreWebView2Environment> environment;
     // Keep an extra webview queued for creation speed
    wil::com_ptr<ICoreWebView2Controller> next_controller;
    wil::com_ptr<ICoreWebView2> next_webview;
    HWND next_hwnd = nullptr;

    void async (std::function<void()>&& f);

    std::vector<std::function<void()>> async_queue;
};

} // namespace win32app
