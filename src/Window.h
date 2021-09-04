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

 // visible_tabs shadow what the shell sees for the update algorithm
 // TODO: move this and shell algorithm stuff to another file
 // Tab IDs are the same as Link IDs, except they allow the value 0 to represent
 //  the view's root page (which has no associated link)
struct Tab {
    LinkID id;
    PageID page; // Not necessarily the link's from_page
    LinkID parent; // The tab this one is shown under
};

struct Window {
    ViewData view;
    std::unordered_map<LinkID, Tab> visible_tabs;

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

