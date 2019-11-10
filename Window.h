#pragma once
#include "stuff.h"

#include <vector>

#include "shell.h"

struct Activity;

// Represents one application window on the desktop.
// This will delete itself when the window is closed.
struct Window {
    HWND hwnd;
    Shell shell;

    std::vector<Activity> activities;

    Window ();

    void set_title (const wchar_t*);

    void shell_ready ();

    void resize_everything ();

    LRESULT window_message (UINT message, WPARAM w, LPARAM l);
};

