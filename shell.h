#pragma once
#include "stuff.h"

#include <wil/com.h>

namespace json { struct Value; }

struct IWebView2WebView;
struct Window;

struct Shell {
    Window* window;
    wil::com_ptr<IWebView2WebView> webview;
    HWND webview_hwnd = nullptr;
    Shell (Window*);

    void interpret_web_message (const json::Value& message);

    void document_state_changed (const wchar_t* url, bool back, bool forward);
};
