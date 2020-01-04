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
struct OSWindow;

struct Window : Observer {
    int64 id;
    int64 focused_tab;
    wil::com_ptr<WebView> webview;
    HWND webview_hwnd = nullptr;

    Activity* activity = nullptr;

    OSWindow os_window;
    Window (int64 id, int64 focused_tab);

     // In DIPs
    double sidebar_width = 240;
    double toolbar_height = 28;
    double main_menu_width = 0;

    void resize ();

    std::function<void()> get_key_handler (uint key, bool shift, bool ctrl, bool alt);

    void Observer_after_commit (
        const std::vector<int64>& updated_tabs,
        const std::vector<int64>& updated_windows
    ) override;

    void send_tabs (const std::vector<int64>& updated_tabs);
    void send_focus ();
    void send_activity ();

    void message_from_shell (json::Value&& message);
    void message_to_shell (json::Value&& message);

    HRESULT on_AcceleratorKeyPressed (IWebView2WebView*, IWebView2AcceleratorKeyPressedEventArgs*);

    void claim_activity (Activity*);

    ~Window();
};
