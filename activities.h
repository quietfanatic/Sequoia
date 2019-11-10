#pragma once

#include <vector>
#include <wil/com.h>

struct IWebView2WebView4;
struct Window;

struct Activity {
    Window* window;
    wil::com_ptr<IWebView2WebView4> page;

    Activity(Window* window);
};

