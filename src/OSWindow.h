#pragma once

#include <windows.h>

struct Activity;

struct Window;

struct OSWindow {
    HWND hwnd;

    bool fullscreen = false;
    WINDOWPLACEMENT placement_before_fullscreen;

    OSWindow(Window* window);

    void set_title (const char*);
    void set_fullscreen (bool);
    void close ();  // Will delete this

    LRESULT WndProc (UINT message, WPARAM w, LPARAM l);
};
