#pragma once

#include "_windows.h"
#include <set>
#include <vector>

#include "shell/shell.h"
#include "stuff.h"

struct Activity;
struct Tab;

// Represents one application window on the desktop.
// This will delete itself when the window is closed.
struct Window {
    HWND hwnd;
    Shell shell;
    Tab* tab = nullptr;
    Activity* activity = nullptr;

    Window ();

    void focus_tab (Tab*);
    void claim_activity (Activity*);
    void set_title (const char16*);
    void resize_everything ();

    LRESULT WndProc (UINT message, WPARAM w, LPARAM l);

    ~Window ();
};
