#include "Window.h"

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
#include "util/utf8.h"

using namespace Microsoft::WRL;
using namespace std;

Window::Window (int64 id, int64 tab) :
    id(id), focused_tab(tab), os_window(this)
{
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
            string raw = to_utf8(raw16.get());
            LOG("message_from_shell", raw);
            message_from_shell(json::parse(raw));
            return S_OK;
        }).Get(), nullptr);

        webview->Navigate(to_utf16(exe_relative("res/shell.html")).c_str());

        resize();
    });
};

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
    if (!os_window.fullscreen) {
        bounds.top += uint(toolbar_height * scale);
        double side_width = sidebar_width > main_menu_width ? sidebar_width : main_menu_width;
        bounds.right -= uint(side_width * scale);
    }
    if (activity) {
        activity->resize(bounds);
    }
}

void Window::Observer_after_commit (
    const vector<int64>& updated_tabs,
    const vector<int64>& updated_windows
) {
    send_tabs(updated_tabs);

    for (auto updated_id : updated_windows) {
        if (updated_id != id) continue;

        int64 old_focus = focused_tab;
        focused_tab = get_window_focused_tab(id);
        if (focused_tab == old_focus) break;

        LOG("focus_tab", focused_tab);
        if (focused_tab) {
            set_tab_visited(focused_tab);
            send_focus();
            claim_activity(ensure_activity_for_tab(focused_tab));
            if (activity->webview && get_tab_data(focused_tab)->url != "about:blank") {
                AH(activity->webview->MoveFocus(WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC));
            }
            send_activity();

            if (old_focus && get_next_unclosed_tab(old_focus) == focused_tab) {
                int64 next = get_next_unclosed_tab(focused_tab);
                if (next) ensure_activity_for_tab(next);
            }
        }
        else claim_activity(nullptr);
        break;
    }
}

void Window::send_tabs (const vector<int64>& updated_tabs) {
    json::Array updates;
    updates.reserve(updated_tabs.size());
    bool do_send_activity = false;
    for (auto tab : updated_tabs) {
        auto t = get_tab_data(tab);
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
            t->title,
            t->url,
            !!activity,
            t->visited_at,
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
        if (focused_tab) {
             // Temporary algorithm until we support expanding and collapsing trees
             // Select all tabs recursively from the root, so that we don't pick up
             // children of closed tabs
            vector<int64> required_tabs = get_all_children(0);
            for (size_t i = 0; i < required_tabs.size(); i++) {
                vector<int64> children = get_all_children(required_tabs[i]);
                for (auto c : children) {
                    required_tabs.push_back(c);
                }
            }
            send_tabs(required_tabs);
            send_focus();
        }
        break;
    }
    case x31_hash("resize"): {
        sidebar_width = uint(message[1]);
        toolbar_height = uint(message[2]);
        resize();
        break;
    }
    case x31_hash("navigate"): {
        const string& address = message[1];
        if (activity) {
            activity->navigate_url_or_search(address);
        }
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
        break;
    }
    case x31_hash("new_child"): {
        int64 tab = message[1];
        Transaction tr;
        int64 new_tab = create_tab(tab, TabRelation::LAST_CHILD, "about:blank");
        set_window_focused_tab(id, new_tab);
        break;
    }
    case x31_hash("delete"): {
        int64 tab = message[1];
        auto data = get_tab_data(tab);
        if (data->closed_at) {
            delete_tab_and_children(tab);
            if (auto activity = activity_for_tab(tab)) {
                delete activity;
            }
        }
        break;
    }
    case x31_hash("close"): {
        int64 tab = message[1];
        auto data = get_tab_data(tab);
        close_tab(tab);
        if (auto activity = activity_for_tab(tab)) {
            delete activity;
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
     // Main menu
    case x31_hash("new_toplevel_tab"): {
        Transaction tr;
        int64 new_tab = create_tab(0, TabRelation::LAST_CHILD, "about:blank");
        set_window_focused_tab(id, new_tab);
        break;
    }
    case x31_hash("fix_problems"): {
        fix_problems();
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

Window::~Window () {
    if (activity) activity->claimed_by_window(nullptr);
}
