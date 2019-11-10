#pragma once
#include "stuff.h"

#include <set>
#include <string>
#include <wil/com.h>

struct IWebView2WebView4;
struct Window;

struct Activity {
    Window* window = nullptr;
    wil::com_ptr<IWebView2WebView4> webview;
    HWND webview_hwnd = nullptr;
    std::wstring url;
    bool can_go_back = false;
    bool can_go_forward = false;

    Activity();
    ~Activity();
};

extern std::set<Activity*> all_activities;
