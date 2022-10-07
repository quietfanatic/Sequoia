#pragma once

#include <functional>
#include <set>
#include <vector>
#include <wil/com.h>
#include <windows.h>
#include <webview2.h>

#include "../model/data.h"
#include "../util/types.h"
#include "oswindow.h"

struct ICoreWebView2AcceleratorKeyPressedEventArgs;
namespace json { struct Value; }

namespace win32app {

struct Window : Observer {
    int64 id;
    wil::com_ptr<ICoreWebView2Controller> shell_controller;
    wil::com_ptr<ICoreWebView2> shell;
    HWND shell_hwnd = nullptr;

     // There are three sets of tabs the window is aware of:
     //  - Expanded tabs
     //  - Visible tabs, children of expanded tabs
     //  - Known tabs, children of visible tabs, grandchildren of expanded tabs
     // Here we only need to store expanded tabs.
     // This set will include the root tab, including if it's the pseudo-tab 0
    std::set<int64> expanded_tabs;

    int64 old_focused_tab = 0;

    Activity* activity = nullptr;
    OSWindow os_window;

     // In DIPs
    double sidebar_width = 240;
    double toolbar_height = 28;
    double main_menu_width = 0;

    bool fullscreen = false;

    void claim_activity (Activity*);
    void hidden ();
    void resize ();
    void enter_fullscreen ();
    void leave_fullscreen ();

     // Controller functions
    HRESULT on_AcceleratorKeyPressed (ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs*);
    std::function<void()> get_key_handler (uint key, bool shift, bool ctrl, bool alt);
    void message_from_shell (json::Value&& message);
    void focus_tab (int64 tab);

    void Observer_after_commit (
        const std::vector<int64>& updated_tabs,
        const std::vector<int64>& updated_windows
    );

     // View functions
    void send_update (const std::vector<int64>& updated_tabs);
    void message_to_shell (json::Value&& message);

    Window (int64 id);
    ~Window();
};

} // namespace win32app
