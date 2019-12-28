#include "Window.h"

#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "activities.h"
#include "assert.h"
#include "data.h"
#include "hash.h"
#include "logging.h"
#include "main.h"
#include "nursery.h"
#include "json/json.h"
#include "settings.h"
#include "utf8.h"
#include "util.h"

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
    if (!os_window.fullscreen) {
        bounds.top += toolbar_height;
        bounds.right -= sidebar_width > main_menu_width ? sidebar_width : main_menu_width;
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
        if (updated_id == id) {
            int64 new_focus = get_window_focused_tab(id);
            if (new_focus != focused_tab) {
                LOG("focus_tab", new_focus);
                int64 old_focus = focused_tab;
                focused_tab = new_focus;
                if (focused_tab) {
                    send_focus();
                    claim_activity(ensure_activity_for_tab(new_focus));
                    if (old_focus) {
                        TabData data = get_tab_data(new_focus);
                        if (data.prev == old_focus) {
                            if (data.next) ensure_activity_for_tab(data.next);
                        }
                    }
                    send_activity();
                }
                else {
                    claim_activity(nullptr);
                }
            }
            break;
        }
    }
}

void Window::send_tabs (const vector<int64>& updated_tabs) {
    json::Array updates;
    updates.reserve(updated_tabs.size());
    bool do_send_activity = false;
    for (auto tab : updated_tabs) {
        TabData t = get_tab_data(tab);
        Activity* activity = activity_for_tab(tab);
        updates.emplace_back(json::array(
            tab,
            t.parent,
            t.prev,
            t.next,
            t.child_count,
            t.title,
            t.url,
            !!activity,
            t.closed_at
        ));
        if (focused_tab == tab) {
            if (t.closed_at) {
                 // If the current tab is closing, find a new tab to focus
                LOG("Finding successor", tab);
                Transaction tr;
                int64 successor;
                while (t.closed_at) {
                    successor = t.next ? t.next
                        : t.parent ? t.parent
                        : t.prev ? t.prev
                        : create_webpage_tab(0, "about:blank");
                    A(successor);
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
    message_to_shell(json::array("focus", focused_tab));
}

void Window::send_activity () {
    if (!activity) return;
    message_to_shell(json::array(
        "activity",
        activity->can_go_back,
        activity->can_go_forward
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
            claim_activity(ensure_activity_for_tab(focused_tab));
            send_activity();
        }
        break;
    }
    case x31_hash("resize"): {
         // TODO: get rasterization scale instead of hardcoding 2 for my laptop
        sidebar_width = uint(message[1]) * 2;
        toolbar_height = uint(message[2]) * 2;
        resize();
        break;
    }
    case x31_hash("navigate"): {
        const string& address = message[1];
        if (auto activity = activity_for_tab(focused_tab)) {
            activity->navigate_url_or_search(address);
        }
        break;
    }
    case x31_hash("back"): {
        if (auto activity = activity_for_tab(focused_tab)) {
            if (activity->webview) activity->webview->GoBack();
        }
        break;
    }
    case x31_hash("forward"): {
        if (auto activity = activity_for_tab(focused_tab)) {
            if (activity->webview) activity->webview->GoForward();
        }
        break;
    }
    case x31_hash("focus"): {
        int64 tab = message[1];
        set_window_focused_tab(id, tab);
        break;
    }
    case x31_hash("close"): {
        int64 tab = message[1];
        close_tab(tab);
        if (auto activity = activity_for_tab(tab)) {
            delete activity;
        }
        break;
    }
    case x31_hash("show_main_menu"): {
        main_menu_width = uint(message[1]) * 2;
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
        int64 new_tab = create_webpage_tab(0, "about:blank");
        set_window_focused_tab(id, new_tab);
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
