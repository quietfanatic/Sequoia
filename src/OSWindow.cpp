#include "Window.h"

#include "util/assert.h"

using namespace std;

static LRESULT CALLBACK WndProcStatic (HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto window = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!window) return DefWindowProc(hwnd, message, w, l);
    switch (message) {
    case WM_SIZE:
    case WM_DPICHANGED:
        window->resize();
        return 0;
    case WM_DESTROY:
         // Prevent activity's HWND from being destroyed.
        window->claim_activity(nullptr);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);
        delete window;
        PostQuitMessage(0);
        return 0;
    }
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

OSWindow::OSWindow (Window* window) : hwnd(create_hwnd()) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
}

void OSWindow::set_title (const char* title) {
    AW(SetWindowText(hwnd, title));
}

void OSWindow::set_fullscreen (bool fs) {
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
