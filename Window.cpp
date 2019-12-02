#include "Window.h"

#include <stdexcept>
#include <WebView2.h>

#include "activities.h"
#include "assert.h"
#include "main.h"
#include "tabs.h"

using namespace std;

namespace MENU { enum {
    NEW_TOP_TAB = 100,
    EXIT,
}; }

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
        AW(RegisterClassEx(&c));
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
    AW(hwnd);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    return hwnd;
}

Window::Window () : hwnd(create_hwnd()), shell(this) {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
}

void Window::focus_tab (Tab* t) {
    if (t == tab) return;
    tab = t;
    if (tab) {
        if (!tab->activity) {
            tab->activity = new Activity(tab);
        }
        claim_activity(tab->activity);
        tab->update();
        Tab::commit();
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
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case MENU::NEW_TOP_TAB: {
            Tab* new_tab = Tab::open_webpage(0, L"about:blank");
            focus_tab(new_tab);
            return 0;
        }
        case MENU::EXIT:
            PostQuitMessage(0);
            return 0;
        }
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

void Window::set_title (const char16* title) {
    AW(SetWindowText(hwnd, title));
}

Window::~Window () {
    claim_activity(nullptr);
}

struct MenuItem {
    UINT id;
    const char16* text;
};

MenuItem main_menu_items [] = {
    {MENU::NEW_TOP_TAB, L"New Toplevel Tab"},
    {MENU::EXIT, L"E&xit"},
};

static HMENU main_menu () {
    static HMENU main_menu = []{
        HMENU main_menu = CreatePopupMenu();
        for (auto item : main_menu_items) {
            MENUITEMINFOW mii = {0};
            mii.cbSize = sizeof(MENUITEMINFOW);
            mii.fMask = MIIM_ID | MIIM_STRING;
            mii.wID = item.id;
            mii.dwTypeData = (LPWSTR)item.text;
            AW(InsertMenuItem(main_menu, 1, TRUE, &mii));
        }
        return main_menu;
    }();
    return main_menu;
}

void Window::show_main_menu (int x, int y) {
    auto m = main_menu();
     // TODO: get rasterization scale instead of hardcoding it
    POINT p {x, y};
    AW(ClientToScreen(hwnd, &p));
    AW(TrackPopupMenuEx(m, TPM_RIGHTALIGN | TPM_TOPALIGN, p.x, p.y, hwnd, nullptr));
}

