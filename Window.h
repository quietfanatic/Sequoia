#pragma once
#include "stuff.h"

#include <vector>

#include "shell/shell.h"

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
    void set_activity (Activity*);
    void update ();
    void resize_everything ();

    LRESULT WndProc (UINT message, WPARAM w, LPARAM l);

    ~Window ();
};

