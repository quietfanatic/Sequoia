#pragma once

#include <windows.h>

#include "../util/types.h"

struct Window;

struct OSWindow {
    HWND hwnd;

    WINDOWPLACEMENT placement_before_fullscreen;

    OSWindow(Window* window);

    void set_title (Str);
    void enter_fullscreen ();
    void leave_fullscreen ();
    void close ();  // Will delete this
};
