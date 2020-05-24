#pragma once

#include <functional>
#include <set>
#include <vector>
#include <wil/com.h>
#include <windows.h>

#include "data.h"
#include "OSWindow.h"
#include "util/types.h"

namespace json { struct Value; }

struct IWebView2WebView;
struct IWebView2AcceleratorKeyPressedEventArgs;

struct Window : Observer {
    int64 id;
    int64 focused_tab;
    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;

     // There are three sets of tabs the window is aware of:
     //  - Expanded tabs
     //  - Visible tabs, children of expanded tabs
     //  - Known tabs, children of visible tabs, grandchildren of expanded tabs
     // Here we only need to store expanded tabs.
     // This set will include the root tab, including if it's the pseudo-tab 0
    std::set<int64> expanded_tabs;

    Activity* activity = nullptr;
    OSWindow os_window;

     // In DIPs
    double sidebar_width = 240;
    double toolbar_height = 28;
    double main_menu_width = 0;

    bool fullscreen = false;

    Window (int64 id);

    void resize ();

    void enter_fullscreen ();
    void leave_fullscreen ();

    std::function<void()> get_key_handler (uint key, bool shift, bool ctrl, bool alt);
    HRESULT on_AcceleratorKeyPressed (IWebView2WebView*, IWebView2AcceleratorKeyPressedEventArgs*);

    void Observer_after_commit (
        const std::vector<int64>& updated_tabs,
        const std::vector<int64>& updated_windows
    );

    void expand_tab (int64 tab);
    void contract_tab (int64 tab);

    void send_root ();
    void send_tabs (const std::vector<int64>& updated_tabs);
    void send_focus ();
    void send_activity ();

    void message_from_shell (json::Value&& message);
    void message_to_shell (json::Value&& message);

    void claim_activity (Activity*);

    ~Window();
};

