#include "Window.h"

#include <stdexcept>
#include <WebView2.h>

#include "activities.h"
#include "assert.h"
#include "data.h"
#include "logging.h"
#include "util.h"

using namespace std;

static LRESULT CALLBACK WndProcStatic (HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto self = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (self) return self->WndProc(message, w, l);
    return DefWindowProc(hwnd, message, w, l);
}

static HWND create_hwnd () {
    static auto class_name = "Sequoia";
    static bool init = []{
        WNDCLASSEX c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.style = CS_HREDRAW | CS_VREDRAW;
        c.lpfnWndProc = WndProcStatic;
        c.hInstance = GetModuleHandle(nullptr);
        c.hCursor = LoadCursor(NULL, IDC_ARROW);
        c.lpszClassName = class_name;
        AW(RegisterClassEx(&c));
        return true;
    }();
    HWND hwnd = CreateWindow(
        class_name,
        "(Sequoia)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1920, 1200,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    AW(hwnd);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    return hwnd;
}

Window::Window (int64 id, int64 tab) : id(id), hwnd(create_hwnd()) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    focus_tab(tab);
}

void Window::focus_tab (int64 t) {
    LOG("focus_tab", t);
    if (t == tab) return;
    int64 old_tab = tab;
    tab = t;
    if (tab) {
        claim_activity(ensure_activity_for_tab(tab));
         // When moving down tab list, preload next tab
        if (old_tab) {
            TabData data = get_tab_data(tab);
            if (data.prev == old_tab) {
                if (data.next) ensure_activity_for_tab(data.next);
            }
        }
        shell.focus_tab(tab);
    }
}

void Window::claim_activity (Activity* a) {
    if (a == activity) return;

    if (activity) activity->claimed_by_window(nullptr);
    activity = a;
    if (activity) activity->claimed_by_window(this);

    resize_everything();
}

LRESULT Window::WndProc (UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_SIZE:
        resize_everything();
        return 0;
    case WM_DESTROY:
         // Prevent activity's hwnd from getting destroyed.
        claim_activity(nullptr);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);
        delete this;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, w, l);
}

void Window::resize_everything () {
    RECT outer;
    GetClientRect(hwnd, &outer);
    RECT inner = shell.resize(outer);
    if (activity) {
        activity->resize(inner);
    };
}

void Window::set_title (const char* title) {
    AW(SetWindowText(hwnd, title));
}

Window::~Window () {
    claim_activity(nullptr);
}

void Window::set_fullscreen (bool fs) {
    if (fs == fullscreen) return;
    fullscreen = fs;
    if (fullscreen) {
        MONITORINFO monitor = {sizeof(MONITORINFO)};
        AW(GetWindowPlacement(hwnd, &placement_before_fullscreen));
        AW(GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor));
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd, HWND_TOP,
            monitor.rcMonitor.left, monitor.rcMonitor.top,
            monitor.rcMonitor.right - monitor.rcMonitor.left,
            monitor.rcMonitor.bottom - monitor.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
    }
    else {
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &placement_before_fullscreen);
        SetWindowPos(
            hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
    }
}
