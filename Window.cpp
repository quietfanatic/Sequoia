#include "Window.h"

#include <stdexcept>
#include <WebView2.h>

#include "activities.h"
#include "assert.h"
#include "main.h"
#include "tabs.h"

using namespace std;

std::set<Window*> all_windows;

static LRESULT CALLBACK WndProcStatic (HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto self = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (self) return self->WndProc(message, w, l);
    return DefWindowProc(hwnd, message, w, l);
}

static HWND create_hwnd () {
    static auto class_name = L"Sequoia";
    static bool init = []{
        WNDCLASSEX c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.style = CS_HREDRAW | CS_VREDRAW;
        c.lpfnWndProc = WndProcStatic;
        c.hInstance = GetModuleHandle(nullptr);
        c.hCursor = LoadCursor(NULL, IDC_ARROW);
        c.lpszClassName = class_name;
        ASSERT(RegisterClassEx(&c));
        return true;
    }();
    HWND hwnd = CreateWindow(
        class_name,
        L"(Sequoia)",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1280, 960,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    ASSERT(hwnd);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    return hwnd;
}

Window::Window () : hwnd(create_hwnd()), shell(this) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    all_windows.emplace(this);
}

void Window::focus_tab (Tab* t) {
    if (t == tab) return;
    tab = t;
    if (tab) {
        if (!tab->activity) {
            tab->activity = new Activity(tab);
        }
        claim_activity(tab->activity);
    }
}

void Window::claim_activity (Activity* a) {
    if (activity && activity != a) activity->claimed_by_window(nullptr);
    activity = a;
    if (activity) activity->claimed_by_window(this);

    resize_everything();
    update_tab(tab);
}

LRESULT Window::WndProc (UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_SIZE:
        resize_everything();
        return 0;
    case WM_DESTROY:
         // Prevent activity's hwnd from getting destroyed.
        claim_activity(nullptr);
    case WM_NCDESTROY:
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);
        delete this;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, message, w, l);
}

void Window::update_tab (Tab* t) {
    shell.update_tab(t);
    if (tab == t) {
        ASSERT(SetWindowText(hwnd, tab ? tab->title.c_str() : L"(Sequoia)"));
    }
}

void Window::resize_everything () {
    RECT outer;
    GetClientRect(hwnd, &outer);
    RECT inner = shell.resize(outer);
    if (activity) {
        activity->resize(inner);
    };
}

Window::~Window () {
    all_windows.erase(this);
    claim_activity(nullptr);
}

