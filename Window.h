#pragma once

#include <set>
#include <vector>
#include <windows.h>

#include "shell.h"
#include "stuff.h"

struct Activity;

// Represents one application window on the desktop.
// This will delete itself when the window is closed.
struct Window {
    int64 id;
    HWND hwnd;
    Shell shell;
    int64 tab = 0;
    Activity* activity = nullptr;

    Window (int64 id, int64 tab);

    void focus_tab (int64);
    void claim_activity (Activity*);
    void set_title (const char*);
    void resize_everything ();
    void show_main_menu (int x, int y);

    LRESULT WndProc (UINT message, WPARAM w, LPARAM l);

    ~Window ();
};
