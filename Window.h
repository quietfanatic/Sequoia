#pragma once
#include "stuff.h"

#include <WebView2.h>
#include <wil/com.h>

// Represents one application window on the desktop.
// This will delete itself when the window is closed.
struct Window {
    HWND hwnd = nullptr;
    wil::com_ptr<IWebView2Environment> webview_environment;
    wil::com_ptr<IWebView2WebView> shell;
    HWND shell_hwnd = nullptr;
    wil::com_ptr<IWebView2WebView> page;
    RECT page_bounds;

    Window ();

    LRESULT window_message (UINT message, WPARAM w, LPARAM l);
    HRESULT on_shell_WebMessageReceived (
        IWebView2WebView* sender,
        IWebView2WebMessageReceivedEventArgs* args
    );
    void resize_everything ();
};

