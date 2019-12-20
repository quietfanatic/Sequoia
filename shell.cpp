#include "shell.h"

#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "activities.h"
#include "assert.h"
#include "data.h"
#include "hash.h"
#include "logging.h"
#include "main.h"
#include "json/json.h"
#include "utf8.h"
#include "util.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;
using namespace json;

Shell::Shell (Window* owner) : window(owner) {
    AH(webview_environment->CreateWebView(window->hwnd,
        Callback<IWebView2CreateWebViewCompletedHandler>(
            [this](HRESULT hr, IWebView2WebView* wv)
    {
        AH(hr);
        AH(wv->QueryInterface(IID_PPV_ARGS(&webview)));
        AW(webview_hwnd = GetWindow(window->hwnd, GW_CHILD));
        EventRegistrationToken token;
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
            message_from_shell(parse(raw));
            return S_OK;
        }).Get(), &token);

        webview->Navigate(to_utf16(exe_relative("res/shell.html")).c_str());

        window->resize_everything();

        return S_OK;
    }).Get()));
};

void Shell::focus_tab (int64 tab) {
    window->focus_tab(tab);
    set_window_focused_tab(window->id, tab);
}

RECT Shell::resize (RECT bounds) {
    if (webview) {
        webview->put_Bounds(bounds);
         // Put shell behind the page
        SetWindowPos(
            webview_hwnd, HWND_BOTTOM,
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
        );
    }
    bounds.top += toolbar_height;
    bounds.right -= sidebar_width;
    return bounds;
}

void Shell::Observer_after_commit (const vector<int64>& updated_tabs) {
    update(updated_tabs);
}

void Shell::update (const vector<int64>& updated_tabs) {
    Array updates;
    updates.reserve(updated_tabs.size());
    for (auto tab : updated_tabs) {
        TabData t = get_tab_data(tab);
        Activity* activity = activity_for_tab(tab);
        if (window->tab == tab) {
            updates.emplace_back(Array{
                tab,
                t.parent,
                t.prev,
                t.next,
                t.child_count,
                t.title,
                t.url,
                !!activity,
                t.closed_at,
                true,
                activity && activity->can_go_back,
                activity && activity->can_go_forward
            });
        }
        else {
            updates.emplace_back(Array{
                tab,
                t.parent,
                t.prev,
                t.next,
                t.child_count,
                t.title,
                t.url,
                !!activity,
                t.closed_at
            });
        }
         // If the current tab is closing, find a new tab to focus
        if (window->tab == tab && t.closed_at) {
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
            focus_tab(successor);
        }
    }
    message_to_shell(Array{"update", updates});
};

static string css_color (uint32 c) {
    char buf [8];
    snprintf(buf, 8, "#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
}

void Shell::message_from_shell (Value&& message) {
    const String& command = message[0];

    switch (x31_hash(command.c_str())) {
    case x31_hash("ready"): {
         // Set system colors
        message_to_shell(Array{
            "colors",
            css_color(GetSysColor(COLOR_ACTIVECAPTION)),
            css_color(GetSysColor(COLOR_CAPTIONTEXT)),
            css_color(GetSysColor(COLOR_3DFACE)),
            css_color(GetSysColor(COLOR_WINDOWTEXT)),
            css_color(GetSysColor(COLOR_3DHIGHLIGHT)),
            css_color(GetSysColor(COLOR_3DSHADOW))
        });
        if (window->tab) {
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
            update(required_tabs);
        }
        break;
    }
    case x31_hash("resize"): {
         // TODO: get rasterization scale instead of hardcoding 2 for my laptop
        sidebar_width = uint(message[1]) * 2;
        toolbar_height = uint(message[2]) * 2;
        window->resize_everything();
        break;
    }
    case x31_hash("navigate"): {
        const String& url = message[1];
        if (auto wv = active_webview()) wv->Navigate(to_utf16(url).c_str());
        break;
    }
    case x31_hash("back"): {
        if (auto wv = active_webview()) wv->GoBack();
        break;
    }
    case x31_hash("forward"): {
        if (auto wv = active_webview()) wv->GoForward();
        break;
    }
    case x31_hash("focus"): {
        int64 id = message[1];
        focus_tab(id);
        break;
    }
    case x31_hash("close"): {
        int64 id = message[1];
        close_tab(id);
        break;
    }
    case x31_hash("main_menu"): {
        int x = message[1];
        int y = message[2];
         // TODO: get rasterization scale instead of hardcoding it
        window->show_main_menu(x * 2, y * 2);
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }
}

void Shell::message_to_shell (Value&& message) {
    if (!webview) return;
    auto s = stringify(message);
    LOG("message_to_shell", s);
    webview->PostWebMessageAsJson(to_utf16(s).c_str());
}

IWebView2WebView4* Shell::active_webview () {
    if (window->activity && window->activity->webview) return window->activity->webview.get();
    else return nullptr;
}
