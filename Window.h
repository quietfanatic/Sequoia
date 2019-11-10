#pragma once
#include "stuff.h"

#include <vector>
#include <WebView2.h>
#include <wil/com.h>

struct Activity;

// Represents one application window on the desktop.
// This will delete itself when the window is closed.
struct Window {
    HWND hwnd = nullptr;
    wil::com_ptr<IWebView2WebView> shell;
    HWND shell_hwnd = nullptr;

    std::vector<Activity> activities;

    Window ();

    void set_title (const wchar_t*);
    void set_url (const wchar_t*);

    void resize_everything ();

    LRESULT window_message (UINT message, WPARAM w, LPARAM l);
    HRESULT on_shell_WebMessageReceived (
        IWebView2WebView* sender,
        IWebView2WebMessageReceivedEventArgs* args
    );
};

