#include "window.h"

#include <unordered_set>
#include <stdexcept>

#include <combaseapi.h>  // Must include combaseapi.h (or wil/com.h) before WebView2.h
#include <windows.h>
#include <WebView2.h>
#include <wrl.h>

#include "../model/actions.h"
#include "../model/link.h"
#include "../model/transaction.h"
#include "../model/view.h"
#include "../util/assert.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "activity.h"
#include "main.h"
#include "nursery.h"
#include "shell.h"

using namespace Microsoft::WRL;
using namespace std;

Window::Window (const model::ViewID& view) :
    view(view), current_view_data(*view), os_window(this)
{
    resize();
};

Window::~Window () = default;

void Window::on_hidden () {
    LOG("Window::on_hidden");
     // TODO: make sure this is actually our page
    Activity* activity = activity_for_page(view->focused_page());
    if (activity && activity->controller) {
        activity->controller->put_IsVisible(FALSE);
    }
    Shell* shell = shell_for_view(view);
    if (shell && shell->controller) {
        shell->controller->put_IsVisible(FALSE);
    }
}

void Window::resize () {
    RECT bounds;
    GetClientRect(os_window.hwnd, &bounds);
    Shell* shell = shell_for_view(view);
    if (shell && shell->controller) {
        AH(shell->controller->put_ParentWindow(os_window.hwnd));
        AH(shell->controller->put_Bounds(bounds));
        AH(shell->controller->put_IsVisible(!view->fullscreen));
    }
    auto dpi = GetDpiForWindow(os_window.hwnd);
    AW(dpi);
    double scale = dpi / 96.0;
    if (!view->fullscreen) {
        bounds.top += uint(toolbar_height * scale);
        double side_width = sidebar_width > main_menu_width ? sidebar_width : main_menu_width;
        bounds.right -= uint(side_width * scale);
    }
     // TODO: make sure this is actually our page
    Activity* activity = activity_for_page(view->focused_page());
    if (activity && activity->controller) {
        AH(activity->controller->put_ParentWindow(os_window.hwnd));
        AH(activity->controller->put_Bounds(bounds));
        AH(activity->controller->put_IsVisible(TRUE));
    }
}

void Window::close () {
    model::close_view(view);
}

std::function<void()> Window::get_key_handler (uint key, bool shift, bool ctrl, bool alt) {
    switch (key) {
    case VK_F11:
        if (!shift && !ctrl && !alt) return [this]{
            model::change_view_fullscreen(view, !view->fullscreen);
        };
        break;
    case 'L':
        if (!shift && ctrl && !alt) return [this]{
             // Skip model for this
            if (Shell* shell = shell_for_view(view)) {
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
            if (view->fullscreen) return [this]{
                model::change_view_fullscreen(view, false);
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
    if (view->fullscreen != current_view_data.fullscreen) {
        if (view->fullscreen) {
            os_window.enter_fullscreen();
            resize();
        }
        else {
            os_window.leave_fullscreen();
            resize();
             // TODO: make sure this is actually our activity
            Activity* activity = activity_for_page(view->focused_page());
            if (activity) activity->leave_fullscreen();
        }
    }
    if (view->focused_page() != current_view_data.focused_page()) {
        page_updated();
    }
    current_view_data = *view;
}

void Window::page_updated () {
    const string& title = view->focused_page()->title;
    os_window.set_title(title.empty() ? "Sequoia" : (title + " – Sequoia").c_str());
}

///// Window registry

static unordered_map<model::ViewID, Window*> windows_by_view;

struct WindowRegistry : model::Observer {
    void Observer_after_commit (
        const model::Update& update
    ) override {
         // Iterate over windows instead over update.views, since a window can
         //  also care if its page changes (for the title)
         // Window's constructor and update should not ever reference any other
         //  windows, so we don't need to be paranoid about ordering.
         // But don't mess with windows_by_view until we're done iterating it
        std::vector<model::ViewID> erase_windows;
        for (auto [view, window] : windows_by_view) {
            if (update.views.count(view)) {
                if (view->exists && !view->closed_at) {
                    window->view_updated();
                }
                else {
                     // Window is owned by the win32 system so don't delete
                     // it yet.
                    LOG("Destroying window", view);
                    AW(DestroyWindow(window->os_window.hwnd));
                    erase_windows.emplace_back(view);
                }
            }
            else if (update.pages.count(view->focused_page())) {
                window->page_updated();
            }
        }
        for (auto view : erase_windows) {
            windows_by_view.erase(view);
        }
    }
};
static WindowRegistry window_registry;

Window* window_for_view (model::ViewID view) {
    auto iter = windows_by_view.find(view);
    if (iter != windows_by_view.end()) return iter->second;
    else return nullptr;
}

Window* window_for_page (model::PageID page) {
    if (page->viewing_view) {
         // TODO: remove cast
        Window* window = window_for_view(model::ViewID{page->viewing_view});
        AA(window);
        return window;
    }
    else return nullptr;
}
