#include "Window.h"

#include <map>
#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "activities.h"
#include "data.h"
#include "main.h"
#include "nursery.h"
#include "settings.h"
#include "util/assert.h"
#include "util/files.h"
#include "util/hash.h"
#include "util/json.h"
#include "util/logging.h"
#include "util/text.h"

using namespace Microsoft::WRL;
using namespace std;

static std::map<int64, Window*> open_windows;

Window::Window (int64 id) :
    id(id), os_window(this)
{
    open_windows.emplace(id, this);
    new_webview([this](WebView* wv, HWND hwnd){
        shell = wv;
        shell_hwnd = hwnd;
        SetParent(shell_hwnd, os_window.hwnd);
        shell->put_IsVisible(TRUE);

        shell->add_WebMessageReceived(
            Callback<IWebView2WebMessageReceivedEventHandler>(
                [this](
                    IWebView2WebView* sender,
                    IWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw16;
            args->get_WebMessageAsJson(&raw16);
            string raw = from_utf16(raw16.get());
            LOG("message_from_shell", raw);
            message_from_shell(json::parse(raw));
            return S_OK;
        }).Get(), nullptr);

        shell->add_AcceleratorKeyPressed(
            Callback<IWebView2AcceleratorKeyPressedEventHandler>(
                this, &Window::on_AcceleratorKeyPressed
            ).Get(), nullptr
        );

        shell->Navigate(to_utf16(exe_relative("res/shell.html")).c_str());

        resize();
    });
};

Window::~Window () {
    if (activity) activity->claimed_by_window(nullptr);
    open_windows.erase(id);
    if (open_windows.empty()) quit();
}

 // An Observer just for creating new windows.
 // Windows do most of the Observing, but they can't create themselves.
struct WindowFactory : Observer {
    void Observer_after_commit (
        const vector<int64>& updated_tabs,
        const vector<int64>& updated_windows
    ) override {
        for (int64 id : updated_windows) {
            auto data = get_window_data(id);
            if (!data->closed_at && !open_windows.count(id)) {
                auto w = new Window (id);
                 // Automatically load focused tab
                w->claim_activity(ensure_activity_for_tab(data->focused_tab));
            }
        }
    }
};
static WindowFactory window_factory;

void Window::claim_activity (Activity* a) {
    if (a == activity) return;

    if (activity) activity->claimed_by_window(nullptr);
    activity = a;
    if (activity) activity->claimed_by_window(this);

    resize();
}

void Window::resize () {
    RECT bounds;
    GetClientRect(os_window.hwnd, &bounds);
    if (shell) {
        shell->put_Bounds(bounds);
         // Put shell behind the page
        SetWindowPos(
            shell_hwnd, HWND_BOTTOM,
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
        );
    }
    auto dpi = GetDpiForWindow(os_window.hwnd);
    AW(dpi);
    double scale = dpi / 96.0;
    if (!fullscreen) {
        bounds.top += uint(toolbar_height * scale);
        double side_width = sidebar_width > main_menu_width ? sidebar_width : main_menu_width;
        bounds.right -= uint(side_width * scale);
    }
    if (activity) {
        activity->resize(bounds);
    }
}

void Window::enter_fullscreen () {
    if (fullscreen) return;
    fullscreen = true;
    os_window.enter_fullscreen();
    shell->put_IsVisible(FALSE);
}

void Window::leave_fullscreen () {
    if (!fullscreen) return;
    fullscreen = false;
    shell->put_IsVisible(TRUE);
    os_window.leave_fullscreen();
    if (activity) activity->leave_fullscreen();
}

HRESULT Window::on_AcceleratorKeyPressed (
    IWebView2WebView* sender,
    IWebView2AcceleratorKeyPressedEventArgs* args
) {
    WEBVIEW2_KEY_EVENT_TYPE type; AH(args->get_KeyEventType(&type));
    switch (type) {
    case WEBVIEW2_KEY_EVENT_TYPE_KEY_DOWN: break;
    case WEBVIEW2_KEY_EVENT_TYPE_SYSTEM_KEY_DOWN: break;
    default: return S_OK;
    }
    UINT key; AH(args->get_VirtualKey(&key));
    INT l; AH(args->get_KeyEventLParam(&l));
    auto handle = get_key_handler(
        key,
        GetKeyState(VK_SHIFT) < 0,
        GetKeyState(VK_CONTROL) < 0,
        GetKeyState(VK_MENU) < 0
    );
    if (handle) {
        AH(args->Handle(TRUE));
        bool repeated = l & (1 << 30);
        if (!repeated) handle();
    }
    return S_OK;
}

std::function<void()> Window::get_key_handler (uint key, bool shift, bool ctrl, bool alt) {
    switch (key) {
    case VK_F11:
        if (!shift && !ctrl && !alt) return [this]{
            fullscreen ? leave_fullscreen() : enter_fullscreen();
        };
        break;
    case 'L':
        if (!shift && ctrl && !alt) return [this]{
            if (!shell) return;
            message_to_shell(json::array("select_location"));
            shell->MoveFocus(WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        };
        break;
    case 'N':
        if (ctrl && !alt) {
            if (shift) return [this]{
                Transaction tr;
                if (int64 w = get_last_closed_window()) {
                    unclose_window(w);
                }
            };
            else return [this]{
                Transaction tr;
                int64 new_tab = create_tab(0, TabRelation::LAST_CHILD, "about:blank");
                int64 new_window = create_window(new_tab, new_tab);
            };
        }
        break;
    case 'T':
        if (ctrl && !alt) {
            if (shift) return [this]{
                Transaction tr;
                if (int64 tab = get_last_closed_tab()) {
                    unclose_tab(tab);
                    set_window_focused_tab(id, tab);
                    claim_activity(ensure_activity_for_tab(tab));
                }
            };
            else return [this]{
                Transaction tr;
                int64 new_tab = create_tab(
                    get_window_data(id)->focused_tab,
                    TabRelation::LAST_CHILD,
                    "about:blank"
                );
                set_window_focused_tab(id, new_tab);
                claim_activity(ensure_activity_for_tab(new_tab));
            };
        }
        break;
    case 'U':
        if (!shift && ctrl && !alt) return [this]{
            if (activity) {
                delete activity;
            }
        };
        break;
    case 'W':
        if (!shift && ctrl && !alt) return [this]{
            close_tab_with_heritage(get_window_data(id)->focused_tab);
        };
        break;
    case VK_TAB:
        if (ctrl && !alt) {
            if (shift) return [this]{
                int64 prev = get_prev_unclosed_tab(get_window_data(id)->focused_tab);
                if (prev) focus_tab(prev);
            };
            else return [this]{
                int64 next = get_next_unclosed_tab(get_window_data(id)->focused_tab);
                if (next) focus_tab(next);
            };
        }
        break;
    }
    return nullptr;
}

void Window::message_from_shell (json::Value&& message) {
    const string& command = message[0];

    switch (x31_hash(command.c_str())) {
    case x31_hash("ready"): {
        message_to_shell(json::array(
            "settings",
            json::Object{
                std::pair{"theme", settings::theme}
            }
        ));
        auto data = get_window_data(id);
         // Focused tab and all its ancestors are expanded.  Send all their
         //  children and grandchildren.
        vector<int64> known_tabs {data->focused_tab};
        for (int64 tab = data->focused_tab;; tab = get_tab_data(tab)->parent) {
            expanded_tabs.emplace(tab);
            for (int64 c : get_all_children(tab)) {
                known_tabs.emplace_back(c);
                for (int64 g : get_all_children(c)) {
                    known_tabs.emplace_back(g);
                }
            }
            if (tab == data->root_tab) break;
        }
        send_update(known_tabs);
        break;
    }
    case x31_hash("resize"): {
        sidebar_width = uint(message[1]);
        toolbar_height = uint(message[2]);
        resize();
        break;
    }
    case x31_hash("navigate"): {
        if (activity) activity->navigate_url_or_search(message[1]);
        break;
    }
     // Toolbar buttons
    case x31_hash("back"): {
        if (activity && activity->webview) {
            activity->webview->GoBack();
        }
        break;
    }
    case x31_hash("forward"): {
        if (activity && activity->webview) {
            activity->webview->GoForward();
        }
        break;
    }
    case x31_hash("reload"): {
        if (activity && activity->webview) {
            activity->webview->Reload();
        }
        break;
    }
    case x31_hash("stop"): {
        if (activity && activity->webview) {
            activity->webview->Stop();
        }
        break;
    }
    case x31_hash("investigate_error"): {
        shell->OpenDevToolsWindow();
        break;
    }
    case x31_hash("show_main_menu"): {
        main_menu_width = uint(message[1]);
        resize();
        break;
    }
    case x31_hash("hide_main_menu"): {
        main_menu_width = 0;
        resize();
        break;
    }
    case x31_hash("new_toplevel_tab"): {
        Transaction tr;
        int64 new_tab = create_tab(0, TabRelation::LAST_CHILD, "about:blank");
        set_window_focused_tab(id, new_tab);
        claim_activity(ensure_activity_for_tab(new_tab));
        break;
    }
     // Tab actions
    case x31_hash("focus"): {
        int64 tab = message[1];
        focus_tab(tab);
        break;
    }
    case x31_hash("new_child"): {
        Transaction tr;
        int64 new_tab = create_tab(message[1], TabRelation::LAST_CHILD, "about:blank");
        set_window_focused_tab(id, new_tab);
        claim_activity(ensure_activity_for_tab(new_tab));
        break;
    }
    case x31_hash("star"): {
        star_tab(message[1]);
        break;
    }
    case x31_hash("unstar"): {
        unstar_tab(message[1]);
        break;
    }
    case x31_hash("close"): {
        int64 tab = message[1];
        Transaction tr;
        if (tab == get_window_data(id)->root_tab) {
            close_window(id);
        }
        else {
            int64 next = get_next_unclosed_tab(get_window_data(id)->focused_tab);
            if (next) focus_tab(next);
        }
        close_tab(tab);
        break;
    }
    case x31_hash("inherit_close"): {
        close_tab_with_heritage(message[1]);
        break;
    }
    case x31_hash("delete"): {
        int64 tab = message[1];
        if (get_tab_data(tab)->closed_at) {
            delete_tab_and_children(tab);
        }
        break;
    }
    case x31_hash("move_tab"): {
        int64 tab = message[1];
        int64 reference = message[2];
        TabRelation rel = TabRelation(uint(message[3]));
        move_tab(tab, reference, rel);
        break;
    }
    case x31_hash("expand"): {
        int64 tab = message[1];
        expanded_tabs.emplace(tab);
        vector<int64> new_known_tabs;
        for (int64 c : get_all_children(tab)) {
            for (int64 g : get_all_children(c)) {
                new_known_tabs.emplace_back(g);
            }
        }
        send_update(new_known_tabs);
        break;
    }
    case x31_hash("contract"): {
        int64 tab = message[1];
        expanded_tabs.erase(tab);
        break;
    }
     // Main menu
    case x31_hash("fullscreen"): {
        fullscreen ? leave_fullscreen() : enter_fullscreen();
        break;
    }
    case x31_hash("fix_problems"): {
        fix_problems();
        break;
    }
    case x31_hash("register_as_browser"): {
        register_as_browser();
        break;
    }
    case x31_hash("quit"): {
        quit();
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }
}

void Window::focus_tab (int64 tab) {
    Transaction tr;
    unclose_tab(tab);
    set_window_focused_tab(id, tab);
     // Load this tab and, if traversing up or down, the next one
    claim_activity(ensure_activity_for_tab(tab));
     // Start the search from old_focused_tab because it could be closed
    if (old_focused_tab) {
        if (get_next_unclosed_tab(old_focused_tab) == tab) {
            if (int64 next = get_next_unclosed_tab(tab)) {
                ensure_activity_for_tab(next);
            }
        }
        else if (get_prev_unclosed_tab(old_focused_tab) == tab) {
            if (int64 prev = get_prev_unclosed_tab(tab)) {
                ensure_activity_for_tab(prev);
            }
        }
    }
}

void Window::Observer_after_commit (
    const vector<int64>& updated_tabs,
    const vector<int64>& updated_windows
) {
    if (get_window_data(id)->closed_at) {
        LOG("Destroying window", id);
        AW(DestroyWindow(os_window.hwnd));
        return;
    }

    send_update(updated_tabs);
}

void Window::send_update (const std::vector<int64>& updated_tabs) {
    auto data = get_window_data(id);

    json::Array updates;
    updates.reserve(updated_tabs.size());

    bool focused_tab_changed = data->focused_tab != old_focused_tab;
    old_focused_tab = data->focused_tab;

    for (int64 tab : updated_tabs) {
        auto t = get_tab_data(tab);
        int64 grandparent = t->parent ? get_tab_data(t->parent)->parent : 0;

         // Only send tab if it is a known tab (child or grandchild of expanded tab)
        if (!expanded_tabs.count(tab)
         && !expanded_tabs.count(t->parent)
         && !expanded_tabs.count(grandparent)
        ) {
            continue;
        }

         // Sending no tab data tells shell to delete tab
        if (t->deleted) {
            updates.emplace_back(json::array(tab));
            continue;
        }

        Activity* activity = activity_for_tab(tab);
        if (activity) {
            updates.emplace_back(json::array(
                tab,
                t->parent,
                t->position.hex(),
                t->child_count,
                t->url,
                t->title,
                t->favicon,
                t->visited_at,
                t->starred_at,
                t->closed_at,
                activity->currently_loading ? 1 : 2,
                activity->can_go_back ? 1 : 0,
                activity->can_go_forward ? 1 : 0
            ));
        }
        else {
            updates.emplace_back(json::array(
                tab,
                t->parent,
                t->position.hex(),
                t->child_count,
                t->url,
                t->title,
                t->favicon,
                t->visited_at,
                t->starred_at,
                t->closed_at
            ));
        }

        if (tab == data->focused_tab) focused_tab_changed = true;
    }

    if (focused_tab_changed) {
        const string& title = get_tab_data(data->focused_tab)->title;
        os_window.set_title(title.empty() ? "Sequoia" : (title + " – Sequoia").c_str());
    }

    message_to_shell(json::array(
        "update",
        data->root_tab,
        data->focused_tab,
        updates
    ));
}

void Window::message_to_shell (json::Value&& message) {
    if (!shell) return;
    auto s = json::stringify(message);
    LOG("message_to_shell", s);
    shell->PostWebMessageAsJson(to_utf16(s).c_str());
}
