#include "stuff.h"

#include <WebView2.h>
#include <wil/com.h>

// Represents one application window on the desktop.
// This will delete itself when the window is closed.
struct Window {
    HWND hwnd;
    wil::com_ptr<IWebView2Environment> webview_environment;
    wil::com_ptr<IWebView2WebView> shell;

    LRESULT window_message (UINT message, WPARAM w, LPARAM l);

    Window ();
};

