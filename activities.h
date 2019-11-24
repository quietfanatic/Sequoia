#pragma once

#include "_windows.h"
#include <set>
#include <string>
#include <wil/com.h>

#include "stuff.h"

struct Tab;
struct Window;

struct Activity {
    Tab* tab;
    Window* window = nullptr;

    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;
    bool can_go_back = false;
    bool can_go_forward = false;

    Activity(Tab*);

    void claimed_by_window (Window*);
    void resize (RECT available);

    ~Activity();
};

extern std::set<Activity*> all_activities;
