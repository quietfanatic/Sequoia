#include "Window.h"

#include <stdexcept>
#include "activities.h"
#include "assert.h"
#include "main.h"

using namespace std;

static LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto self = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (self) return self->window_message(message, w, l);
    return DefWindowProc(hwnd, message, w, l);
}

static HWND create_hwnd (Window* owner) {
    auto class_name = L"Sequoia";
    static bool init = []{
        WNDCLASSEX c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.style = CS_HREDRAW | CS_VREDRAW;
        c.lpfnWndProc = WndProc;
        c.hInstance = GetModuleHandle(nullptr);
        c.hCursor = LoadCursor(NULL, IDC_ARROW);
        c.lpszClassName = L"Sequoia";
        ASSERT(RegisterClassEx(&c));
        return true;
    }();
    HWND hwnd = CreateWindow(
        class_name,
        L"Sequoia",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 1280,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    ASSERT(hwnd);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)owner);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    return hwnd;
}

Window::Window () : hwnd(create_hwnd(this)), shell(this) { }

void Window::set_title (const wchar_t* title) {
    ASSERT(SetWindowText(hwnd, title));
}

LRESULT Window::window_message (UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_SIZE:
        resize_everything();
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);
        delete this;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, w, l);
}

void Window::shell_ready () {
    activities.emplace_back(this);
}

void Window::resize_everything () {
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    if (shell.webview) {
        shell.webview->put_Bounds(bounds);
        SetWindowPos(
            shell.webview_hwnd, HWND_BOTTOM,
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
        );
    };
    if (activities.size()) {
        bounds.top += 68;
        bounds.right -= 244;
        activities[0].page->put_Bounds(bounds);
    };
}
