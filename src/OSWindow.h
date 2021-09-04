#pragma once

#include <windows.h>

struct Activity;

struct Window;

struct OSWindow {
    HWND hwnd;

    WINDOWPLACEMENT placement_before_fullscreen;

    OSWindow(Window* window);

    void set_title (const char*);
    void enter_fullscreen ();
    void leave_fullscreen ();
    void close ();  // Will delete this
};
