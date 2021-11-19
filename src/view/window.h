#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <wil/com.h>
#include <windows.h>

#include "../model/tabs.h"
#include "../model/view.h"
#include "../util/types.h"
#include "os_window.h"

namespace json { struct Value; }

struct ICoreWebView2AcceleratorKeyPressedEventArgs;

struct Window {
    model::ViewID view;
    model::TabTree tabs;

    wil::com_ptr<ICoreWebView2Controller> shell_controller;
    wil::com_ptr<ICoreWebView2> shell;
    HWND shell_hwnd = nullptr;
    bool ready = false;

     // reflects view->fullscreen
    bool currently_fullscreen = false;

    OSWindow os_window;

     // In DIPs
    double sidebar_width = 240;
    double toolbar_height = 28;
    double main_menu_width = 0;

     // Window functions
    void on_hidden ();
    void resize ();
    void close();

     // Controller functions
    HRESULT on_AcceleratorKeyPressed (ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs*);
    std::function<void()> get_key_handler (uint key, bool shift, bool ctrl, bool alt);
    void message_from_shell (json::Value&& message);

    void send_view ();
    void update (model::ViewID, const model::Update&);
    void message_to_shell (json::Value&& message);

    Window (const model::ViewID&);
    ~Window();
};

Window* window_for_view (model::ViewID);
Window* window_for_page (model::PageID);
