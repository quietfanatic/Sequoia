#pragma once

#include <windows.h>

namespace win32app {
struct Activity;
struct Bark;

struct OSWindow {
    HWND hwnd;

    WINDOWPLACEMENT placement_before_fullscreen;

    OSWindow(Bark*);

    void set_title (const char*);
    void enter_fullscreen ();
    void leave_fullscreen ();
    void close ();  // Will delete this

    LRESULT WndProc (UINT message, WPARAM w, LPARAM l);
};

} // namespace win32app
