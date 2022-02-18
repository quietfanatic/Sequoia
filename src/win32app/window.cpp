#include "window.h"

#include <unordered_set>
#include <stdexcept>

 // Must include combaseapi.h (or wil/com.h) before WebView2.h
#include <combaseapi.h>
#include <windows.h>
#include <WebView2.h>
#include <wrl.h>

#include "../model/edge.h"
#include "../model/node.h"
#include "../model/view.h"
#include "../model/write.h"
#include "../util/error.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "activity.h"
#include "activity_collection.h"
#include "app.h"
#include "nursery.h"
#include "shell.h"

using namespace Microsoft::WRL;
using namespace std;

namespace win32app {

static LRESULT CALLBACK window_WndProc (HWND, UINT, WPARAM, LPARAM);

Window::Window (App& a, model::ViewID v) :
    app(a), view(v),
    current_focused_node(focused_node(app.model, view))
{
    static auto class_name = L"Sequoia";
    static bool init = []{
        WNDCLASSEXW c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.style = CS_HREDRAW | CS_VREDRAW;
        c.lpfnWndProc = window_WndProc;
        c.hInstance = GetModuleHandle(nullptr);
        c.hCursor = LoadCursor(NULL, IDC_ARROW);
        c.lpszClassName = class_name;
        AW(RegisterClassExW(&c));
        return true;
    }();
    hwnd = CreateWindowW(
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
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
    ShowWindow(hwnd, SW_SHOWDEFAULT);
};

Window::~Window () {
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)nullptr);
    DestroyWindow(hwnd);
}

void Window::reflow () {
    RECT bounds;
    GetClientRect(hwnd, &bounds);
    Shell* shell = app.shell_for_view(view);
    if (shell && shell->controller) {
        AH(shell->controller->put_ParentWindow(hwnd));
        AH(shell->controller->put_Bounds(bounds));
        AH(shell->controller->put_IsVisible(!fullscreen));
    }
    auto dpi = GetDpiForWindow(hwnd);
    AW(dpi);
    double scale = dpi / 96.0;
    if (!fullscreen) {
        bounds.top += uint(toolbar_height * scale);
        double side_width = sidebar_width > main_menu_width
            ? sidebar_width : main_menu_width;
        bounds.right -= uint(side_width * scale);
    }
     // TODO: make sure this is actually our node
    Activity* activity = app.activities->get_for_node(current_focused_node);
    if (activity && activity->controller) {
        AH(activity->controller->put_ParentWindow(hwnd));
        AH(activity->controller->put_Bounds(bounds));
        AH(activity->controller->put_IsVisible(TRUE));
    }
}

static LRESULT CALLBACK window_WndProc (
    HWND hwnd, UINT message, WPARAM w, LPARAM l
) {
    auto window = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!window) return DefWindowProc(hwnd, message, w, l);
    switch (message) {
        case WM_SIZE:
            switch (w) {
                case SIZE_MINIMIZED: {
                    LOG("Window minimized"sv);
                     // TODO: make sure this is actually our node
                    Activity* activity = window->app.activities->get_for_node(
                        window->current_focused_node
                    );
                    if (activity && activity->controller) {
                        activity->controller->put_IsVisible(FALSE);
                    }
                    Shell* shell = window->app.shell_for_view(window->view);
                    if (shell && shell->controller) {
                        shell->controller->put_IsVisible(FALSE);
                    }
                }
                default:
                    window->reflow();
                    break;
            }
            return 0;
        case WM_DPICHANGED:
            window->reflow();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
             // Since our application is only webviews, I'm not sure we ever
             // get here
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
             // TODO: respond to session manager messages
            close(write(window->app.model), window->view);
            return 0;
    }
    return DefWindowProc(hwnd, message, w, l);
}

std::function<void()> Window::get_key_handler (
    uint key, bool shift, bool ctrl, bool alt
) {
    switch (key) {
    case VK_F11:
        if (!shift && !ctrl && !alt) return [this]{
            set_fullscreen(write(app.model), view, !fullscreen);
        };
        break;
    case 'L':
        if (!shift && ctrl && !alt) return [this]{
             // Skip model for this
            if (Shell* shell = app.shell_for_view(view)) {
                shell->select_location();
            }
        };
        break;
    case 'N':
        if (ctrl && !alt) {
            if (shift) return [this]{
                // TODO: unclose window
            };
            else return [this]{
                // TODO: new window
            };
        }
        break;
    case 'T':
        if (ctrl && !alt) {
            if (shift) return [this]{
                // TODO: unclose tab
            };
            else return [this]{
                // TODO: new tab
            };
        }
        break;
    case 'U':
        if (!shift && ctrl && !alt) return [this]{
            // TODO: unload
        };
        break;
    case 'W':
        if (!shift && ctrl && !alt) return [this]{
            // TODO: close tab
        };
        break;
    case VK_TAB:
        if (ctrl && !alt) {
            if (shift) return [this]{
                // TODO: prev tab
            };
            else return [this]{
                // TODO: next tab
            };
        }
        break;
    case VK_ESCAPE:
        if (!shift && !ctrl && !alt) {
            if (fullscreen) return [this]{
                set_fullscreen(write(app.model), view, false);
            };
        }
        break;
    case VK_UP:
        if (!shift && ctrl && !alt) {
            return [this]{
                // TODO: prev tab
            };
        }
        break;
    case VK_DOWN:
        if (!shift && ctrl && !alt) {
            return [this]{
                // TODO: next tab
            };
        }
        break;
    }
    return nullptr;
}

void Window::view_updated () {
    auto view_data = app.model/view;
    if (view_data->fullscreen && !fullscreen) {
        fullscreen = Fullscreen{};
        MONITORINFO monitor = {sizeof(MONITORINFO)};
        AW(GetWindowPlacement(hwnd, &fullscreen->old_placement));
        AW(GetMonitorInfo(
            MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &monitor
        ));
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd, HWND_TOP,
            monitor.rcMonitor.left, monitor.rcMonitor.top,
            monitor.rcMonitor.right - monitor.rcMonitor.left,
            monitor.rcMonitor.bottom - monitor.rcMonitor.top,
            SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
        reflow();
    }
    else if (!view_data->fullscreen && fullscreen) {
        DWORD style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &fullscreen->old_placement);
        SetWindowPos(
            hwnd, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
        );
        reflow();
         // TODO: make sure this is actually our activity
        Activity* activity = app.activities->get_for_node(current_focused_node);
        if (activity) activity->leave_fullscreen();
        fullscreen = nullopt;
    }
    auto new_focused_node = focused_node(app.model, view);
    if (new_focused_node != current_focused_node) {
        node_updated();
    }
    current_focused_node = new_focused_node;
}

void Window::node_updated () {
    Str title = (app.model/focused_node(app.model, view))->title;
    String display = title.empty() ? "Sequoia"s : (title + " – Sequoia"sv);
    AW(SetWindowTextW(hwnd, to_utf16(display).c_str()));
}

} // namespace win32app
