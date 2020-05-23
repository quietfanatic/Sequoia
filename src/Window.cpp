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
    id(id), focused_tab(get_window_data(id)->focused_tab), os_window(this)
{
    open_windows.emplace(id, this);
    new_webview([this](WebView* wv, HWND hwnd){
        webview = wv;
        webview_hwnd = hwnd;
        SetParent(webview_hwnd, os_window.hwnd);
        webview->put_IsVisible(TRUE);

        webview->add_WebMessageReceived(
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

        webview->add_AcceleratorKeyPressed(
            Callback<IWebView2AcceleratorKeyPressedEventHandler>(
                this, &Window::on_AcceleratorKeyPressed
            ).Get(), nullptr
        );

        webview->Navigate(to_utf16(exe_relative("res/shell.html")).c_str());

        resize();
    });
};

struct WindowObserver : Observer {
    void Observer_after_commit (
        const vector<int64>& updated_tabs,
        const vector<int64>& updated_windows
    ) override {
        for (auto id : updated_windows) {
            auto it = open_windows.find(id);
            if (it != open_windows.end()) {
                it->second->update(updated_tabs);
                 // Window may have destroyed itself, so don't do anything more with it.
            }
            else {
                 // Couldn't find existing window for this ID, so it must be new.
                auto w = new Window (id);
                 // Automatically load focused tab
                w->claim_activity(ensure_activity_for_tab(w->focused_tab));
            }
        }
    }
};
WindowObserver window_observer;

void Window::resize () {
    RECT bounds;
    GetClientRect(os_window.hwnd, &bounds);
    if (webview) {
        webview->put_Bounds(bounds);
         // Put shell behind the page
        SetWindowPos(
            webview_hwnd, HWND_BOTTOM,
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
    webview->put_IsVisible(FALSE);
}

void Window::leave_fullscreen () {
    if (!fullscreen) return;
    fullscreen = false;
    webview->put_IsVisible(TRUE);
    os_window.leave_fullscreen();
    if (activity) activity->leave_fullscreen();
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
            if (!webview) return;
            message_to_shell(json::array("select_location"));
            webview->MoveFocus(WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        };
        break;
    case 'N':
        if (!shift && ctrl && !alt) return [this]{
            int64 new_tab = create_tab(0, TabRelation::LAST_CHILD, "about:blank");
            int64 new_window = create_window(new_tab);
        };
        break;
    case 'T':
        if (ctrl && !alt) {
            if (shift) return [this]{
                Transaction tr;
                int64 tab = get_last_closed_tab();
                if (tab) {
                    unclose_tab(tab);
                    set_window_focused_tab(id, tab);
                }
            };
            else return [this]{
                Transaction tr;
                int64 new_tab = create_tab(focused_tab, TabRelation::LAST_CHILD, "about:blank");
                set_window_focused_tab(id, new_tab);
            };
        }
        break;
    case 'U':
        if (!shift && ctrl && !alt) {
            return [this]{
                if (focused_tab && activity) {
                    delete activity;
                    send_tabs(vector{focused_tab});
                    send_activity();
                }
            };
        }
        break;
    case 'W':
        if (!shift && ctrl && !alt) return [this]{
            close_tab_with_heritage(focused_tab);
        };
        break;
    case VK_TAB:
        if (ctrl && !alt) {
            if (shift) return [this]{
                if (int64 prev = get_prev_unclosed_tab(focused_tab)) {
                    set_window_focused_tab(id, prev);
                }
            };
            else return [this]{
                if (int64 next = get_next_unclosed_tab(focused_tab)) {
                    set_window_focused_tab(id, next);
                }
            };
        }
        break;
    }
    return nullptr;
}

void Window::update (
    const vector<int64>& updated_tabs
) {
    if (get_window_data(id)->closed_at) {
        LOG("Destroying window", id);
        AW(DestroyWindow(os_window.hwnd));
        return;
    }

    send_tabs(updated_tabs);

    int64 old_focus = focused_tab;
    focused_tab = get_window_data(id)->focused_tab;
    if (focused_tab != old_focus) {
        LOG("focus_tab", focused_tab);
        if (focused_tab) {
            set_tab_visited(focused_tab);
            send_focus();
            claim_activity(ensure_activity_for_tab(focused_tab));
            if (get_tab_data(focused_tab)->url != "about:blank") {
                if (activity->webview) {
                    AH(activity->webview->MoveFocus(WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC));
                }
            }
            else {
                if (webview) {
                    message_to_shell(json::array("select_location"));
                    webview->MoveFocus(WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
                }
            }
            send_activity();

            if (old_focus && get_next_unclosed_tab(old_focus) == focused_tab) {
                int64 next = get_next_unclosed_tab(focused_tab);
                if (next) ensure_activity_for_tab(next);
            }
        }
        else claim_activity(nullptr);
    }

    for (auto tab : updated_tabs) {
        if (tab == focused_tab) {
            send_focus();
            break;
        }
    }
}

void Window::send_tabs (const vector<int64>& updated_tabs) {
    if (updated_tabs.empty()) return;
    json::Array updates;
    updates.reserve(updated_tabs.size());
    bool do_send_activity = false;

    for (auto tab : updated_tabs) {
        auto t = get_tab_data(tab);

         // Only send tab if it is a known tab (child or grandchild of expanded tab)
        if (!expanded_tabs.count(t->parent)
            && t->parent && !expanded_tabs.count(get_tab_data(t->parent)->parent)
        ) {
            continue;
        }

         // Sending no tab data tells shell to delete tab
        if (t->deleted) {
            updates.emplace_back(json::array(tab));
            continue;
        }

        Activity* activity = activity_for_tab(tab);
        updates.emplace_back(json::array(
            tab,
            t->parent,
            t->position.hex(),
            t->child_count,
            t->url,
            t->title,
            t->favicon,
            !!activity,
            t->visited_at,
            t->starred_at,
            t->closed_at
        ));

        if (focused_tab == tab) {
            if (t->closed_at) {
                 // If the current tab is closing, find a new tab to focus
                LOG("Finding successor", tab);
                Transaction tr;
                int64 successor;
                while (t->closed_at) {
                    successor = get_next_unclosed_tab(tab);
                    if (!successor) successor = t->parent;
                    if (!successor) successor = get_prev_unclosed_tab(tab);
                    if (!successor) successor = create_tab(0, TabRelation::LAST_CHILD, "about:blank");
                    t = get_tab_data(successor);
                }
                set_window_focused_tab(id, successor);
            }
            else {
                do_send_activity = true;
            }
        }
    }

    message_to_shell(json::array("tabs", updates));
    if (do_send_activity) send_activity();
};

void Window::send_focus () {
    const string& title = get_tab_data(focused_tab)->title;
    os_window.set_title(title.empty() ? "Sequoia" : (title + " – Sequoia").c_str());
    message_to_shell(json::array("focus", focused_tab));
}

void Window::send_activity () {
    if (!activity) return;
    message_to_shell(json::array(
        "activity",
        activity->can_go_back,
        activity->can_go_forward,
        activity->currently_loading
    ));
}

static string css_color (uint32 c) {
    char buf [8];
    snprintf(buf, 8, "#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
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
        A(focused_tab);
         // Focused tab and all its ancestors are expanded.  Send all their
         //  children.  Grandchildren will be requested later by the shell with
         //  "expand" commands, including one for root (0).
        vector<int64> known_tabs;
        for (int64 tab = focused_tab;; tab = get_tab_data(tab)->parent) {
            expanded_tabs.emplace(tab);
            for (auto c : get_all_children(tab)) {
                known_tabs.emplace_back(c);
            }
            if (!tab) break;
        }
        send_tabs(known_tabs);
        send_focus();
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
        webview->OpenDevToolsWindow();
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
        break;
    }
     // Tab actions
    case x31_hash("focus"): {
        int64 tab = message[1];
        Transaction tr;
        unclose_tab(tab);
        set_window_focused_tab(id, tab);
        break;
    }
    case x31_hash("load"): {
        int64 tab = message[1];
        Activity* a = ensure_activity_for_tab(tab);
        if (tab == focused_tab) {
            claim_activity(a);
            send_activity();
        }
        Transaction tr;
        tab_updated(tab);
        break;
    }
    case x31_hash("new_child"): {
        Transaction tr;
        int64 new_tab = create_tab(message[1], TabRelation::LAST_CHILD, "about:blank");
        set_window_focused_tab(id, new_tab);
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
        close_tab(message[1]);
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
        for (auto c : get_all_children(tab)) {
            for (auto g : get_all_children(c)) {
                new_known_tabs.emplace_back(g);
            }
        }
        send_tabs(new_known_tabs);
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

void Window::message_to_shell (json::Value&& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_shell", s);
    webview->PostWebMessageAsJson(to_utf16(s).c_str());
}

void Window::claim_activity (Activity* a) {
    if (a == activity) return;

    if (activity) activity->claimed_by_window(nullptr);
    activity = a;
    if (activity) activity->claimed_by_window(this);

    resize();
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

Window::~Window () {
    if (activity) activity->claimed_by_window(nullptr);
    open_windows.erase(id);
    if (open_windows.empty()) quit();
}
