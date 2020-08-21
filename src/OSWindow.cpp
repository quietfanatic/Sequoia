#include "Window.h"

#include "data.h"
#include "util/assert.h"
#include "util/logging.h"
#include "util/text.h"

using namespace std;

static LRESULT CALLBACK WndProcStatic (HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto window = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!window) return DefWindowProc(hwnd, message, w, l);
    switch (message) {
    case WM_SIZE:
        switch (w) {
        case SIZE_MINIMIZED:
            window->hidden();
        default:
            window->resize();
        }
        return 0;
    case WM_DPICHANGED:
        window->resize();
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
         // Since our application is only webviews, I'm not we ever get here
        auto handler = window->get_key_handler(
            uint(w),
            GetKeyState(VK_SHIFT) < 0,
            GetKeyState(VK_CONTROL) < 0,
            GetKeyState(VK_MENU) < 0
        );
        if (handler) {
            bool repeated = l & (1 << 30);
            if (!repeated) handler();
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        close_window(window->id);
        return 0;
    case WM_DESTROY:
         // Prevent activity's HWND from being destroyed.
        window->claim_activity(nullptr);
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);
        delete window;
        return 0;
    }
    return DefWindowProc(hwnd, message, w, l);
}

static HWND create_hwnd () {
    static auto class_name = L"Sequoia";
    static bool init = []{
        WNDCLASSEXW c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.style = CS_HREDRAW | CS_VREDRAW;
        c.lpfnWndProc = WndProcStatic;
        c.hInstance = GetModuleHandle(nullptr);
        c.hCursor = LoadCursor(NULL, IDC_ARROW);
        c.lpszClassName = class_name;
        AW(RegisterClassExW(&c));
        return true;
    }();
    HWND hwnd = CreateWindowW(
        class_name,
        L"Sequoia",
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
    AW(SetWindowTextW(hwnd, to_utf16(title).c_str()));
}

void OSWindow::enter_fullscreen () {
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
void OSWindow::leave_fullscreen () {
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(hwnd, &placement_before_fullscreen);
    SetWindowPos(
        hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
    );
}
