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
    HWND hwnd;
    Shell shell;
    int64 tab = 0;
    Activity* activity = nullptr;

    Window ();

    void focus_tab (int64);
    void claim_activity (Activity*);
    void set_title (const char*);
    void resize_everything ();
    void show_main_menu (int x, int y);

    LRESULT WndProc (UINT message, WPARAM w, LPARAM l);

    ~Window ();
};
