#pragma once
#include "stuff.h"

#include <wil/com.h>

namespace json { struct Value; }

struct IWebView2WebView4;
struct Window;

struct Shell {
    Window* window;
    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;
    Shell (Window*);

    void interpret_web_message (const json::Value& message);
    void update ();
    RECT resize (RECT available);

    IWebView2WebView4* active_webview ();
};
