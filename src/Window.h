#pragma once

#include <functional>
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
    ) override;

    void send_tabs (const std::vector<int64>& updated_tabs);
    void send_focus ();
    void send_activity ();

    void message_from_shell (json::Value&& message);
    void message_to_shell (json::Value&& message);

    void claim_activity (Activity*);

    ~Window();
};

