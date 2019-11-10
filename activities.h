#pragma once

#include <vector>
#include <WebView2.h>
#include <wil/com.h>

struct Window;

struct Activity {
    Window* window;
    wil::com_ptr<IWebView2WebView4> page;

    Activity(Window* window);
};

