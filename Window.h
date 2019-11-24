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
    void update_tab (Tab* t);
    void resize_everything ();

    LRESULT WndProc (UINT message, WPARAM w, LPARAM l);

    ~Window ();
};

extern std::set<Window*> all_windows;
