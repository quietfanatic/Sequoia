#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <wil/com.h>
#include <windows.h>

#include "data.h"
#include "OSWindow.h"
#include "util/types.h"

namespace json { struct Value; }

struct ICoreWebView2AcceleratorKeyPressedEventArgs;

 // Mirrors the structure of the tree view in the shell.
 //  I hate to resort to using a shadow dom to manage updates because of the
 //  overhead, but it really is the easiest algorithm to work with.
 //  ...and let's be real, this is ants compared to the js dom.
struct Tab {
     // Must match constants in shell.js
    enum Flags {
        FOCUSED = 1,
        VISITED = 2,
        LOADING = 4,
        LOADED = 8,
        TRASHED = 16,
        EXPANDABLE = 32,
        EXPANDED = 64,
    };
    PageID page;
    LinkID parent;
    Flags flags;
};

struct Window {
    ViewData view;
    std::unordered_map<LinkID, Tab> tabs;

    wil::com_ptr<ICoreWebView2Controller> shell_controller;
    wil::com_ptr<ICoreWebView2> shell;
    HWND shell_hwnd = nullptr;
    bool ready = false;

    Activity* activity = nullptr;
    OSWindow os_window;

     // In DIPs
    double sidebar_width = 240;
    double toolbar_height = 28;
    double main_menu_width = 0;

    bool fullscreen = false;

     // Window functions
    void claim_activity (Activity*);
    void on_hidden ();
    void resize ();
    void enter_fullscreen ();
    void leave_fullscreen ();
    void close();

     // Controller functions
    HRESULT on_AcceleratorKeyPressed (ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs*);
    std::function<void()> get_key_handler (uint key, bool shift, bool ctrl, bool alt);
    void message_from_shell (json::Value&& message);

     // View functions
    void send_view ();
    void send_update (const Update&);
    void message_to_shell (json::Value&& message);

    Window (ViewID);
    ~Window();
};

