#pragma once

#include <vector>
#include <wil/com.h>
#include <windows.h>

#include "data.h"
#include "stuff.h"
#include "OSWindow.h"

namespace json { struct Value; }

struct OSWindow;

struct Window : Observer {
    int64 id;
    int64 focused_tab;
    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;

    Activity* activity = nullptr;

    OSWindow os_window;
    Window (int64 id, int64 focused_tab);

     // These are in device pixels, not dips
    uint sidebar_width = 480;
    uint toolbar_height = 56;
    uint main_menu_width = 0;

    void resize ();

    void Observer_after_commit (
        const std::vector<int64>& updated_tabs,
        const std::vector<int64>& updated_windows
    ) override;

    void send_tabs (const std::vector<int64>& updated_tabs);
    void send_focus ();
    void send_activity ();

    void message_from_shell (json::Value&& message);
    void message_to_shell (json::Value&& message);

    void claim_activity (Activity*);

    ~Window();
};
