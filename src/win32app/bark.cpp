#include "bark.h"

#include <map>
#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "../model/data.h"
#include "../util/error.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "activities.h"
#include "app.h"
#include "nursery.h"
#include "profile.h"

using namespace Microsoft::WRL;
using namespace std;

namespace win32app {

Bark::Bark (App& app, int64 id) :
    app(app), id(id), os_window(this)
{
    app.nursery.new_webview([this](ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND h){
        controller = wvc;
        webview = wv;
        webview_hwnd = h;
        SetParent(webview_hwnd, os_window.hwnd);
        controller->put_IsVisible(TRUE);

        webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](
                    ICoreWebView2* sender,
                    ICoreWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw16;
            args->get_WebMessageAsJson(&raw16);
            String raw = from_utf16(raw16.get());
            LOG("message_from_shell", raw);
            message_from_shell(json::parse(raw));
            return S_OK;
        }).Get(), nullptr);

        controller->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                this, &Bark::on_AcceleratorKeyPressed
            ).Get(), nullptr
        );

        webview->Navigate(to_utf16(exe_relative("res/win32app/bark.html")).c_str());

        resize();
    });
};

Bark::~Bark () {
    if (activity) activity->claimed_by_bark(nullptr);
}

void Bark::claim_activity (Activity* a) {
    if (a == activity) return;

    if (activity) activity->claimed_by_bark(nullptr);
    activity = a;
    if (activity) activity->claimed_by_bark(this);

    resize();
}

void Bark::hidden () {
    LOG("Bark::hidden");
    if (activity && activity->controller) {
        activity->controller->put_IsVisible(FALSE);
    }
    if (controller) controller->put_IsVisible(TRUE);
}

void Bark::resize () {
    if (activity && activity->controller) {
        activity->controller->put_IsVisible(TRUE);
    }
    if (controller) controller->put_IsVisible(TRUE);

    RECT bounds;
    GetClientRect(os_window.hwnd, &bounds);
    if (controller) {
        controller->put_Bounds(bounds);
         // Put webview behind the page
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

void Bark::enter_fullscreen () {
    if (fullscreen) return;
    if (!activity) return;
    fullscreen = true;
    os_window.enter_fullscreen();
    controller->put_IsVisible(FALSE);
}

void Bark::leave_fullscreen () {
    if (!fullscreen) return;
    fullscreen = false;
    controller->put_IsVisible(TRUE);
    os_window.leave_fullscreen();
    if (activity) activity->leave_fullscreen();
}

HRESULT Bark::on_AcceleratorKeyPressed (
    ICoreWebView2Controller* sender,
    ICoreWebView2AcceleratorKeyPressedEventArgs* args
) {
    COREWEBVIEW2_KEY_EVENT_KIND kind;
    AH(args->get_KeyEventKind(&kind));
    switch (kind) {
    case COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN: break;
    case COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN: break;
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
        AH(args->put_Handled(TRUE));
        bool repeated = l & (1 << 30);
        if (!repeated) handle();
    }
    return S_OK;
}

std::function<void()> Bark::get_key_handler (uint key, bool shift, bool ctrl, bool alt) {
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
            controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        };
        break;
    case 'N':
        if (ctrl && !alt) {
            if (shift) return [this]{
                Transaction tr;
                if (int64 w = get_last_closed_window()) {
                    unclose_window(w);
                    unclose_tab(get_window_data(w)->focused_tab);
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
                    claim_activity(app.ensure_activity_for_tab(tab));
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
                claim_activity(app.ensure_activity_for_tab(new_tab));
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
    case VK_ESCAPE:
        if (!shift && !ctrl && !alt) {
            if (fullscreen) return [this]{
                leave_fullscreen();
            };
        }
        break;
    case VK_UP:
        if (!shift && ctrl && !alt) {
            return [this]{
                int64 prev = get_prev_unclosed_tab(get_window_data(id)->focused_tab);
                if (prev) focus_tab(prev);
            };
        }
        break;
    case VK_DOWN:
        if (!shift && ctrl && !alt) {
            return [this]{
                int64 next = get_next_unclosed_tab(get_window_data(id)->focused_tab);
                if (next) focus_tab(next);
            };
        }
        break;
    }
    return nullptr;
}

void Bark::message_from_shell (json::Value&& message) {
    Str command = message[0];

    switch (x31_hash(command)) {
    case x31_hash("ready"): {
        message_to_shell(json::array(
            "settings",
            json::Object{
                std::pair{"theme", app.settings.theme}
            }
        ));
        auto data = get_window_data(id);
         // Focused tab and all its ancestors are expanded.  Send all their
         //  children and grandchildren.
         // TODO: initialize expanded tabs in constructor
        vector<int64> known_tabs;
        for (int64 tab = data->focused_tab;; tab = get_tab_data(tab)->parent) {
            expanded_tabs.emplace(tab);
            known_tabs.emplace_back(tab);
            for (int64 c : get_all_children(tab)) {
                known_tabs.emplace_back(c);
                for (int64 g : get_all_children(c)) {
                    known_tabs.emplace_back(g);
                }
            }
            if (tab == data->root_tab || !tab) break;
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
        webview->OpenDevToolsWindow();
        break;
    }
    case x31_hash("show_menu"): {
        main_menu_width = uint(message[1]);
        resize();
        break;
    }
    case x31_hash("hide_menu"): {
        main_menu_width = 0;
        resize();
        break;
    }
    case x31_hash("new_toplevel_tab"): {
        Transaction tr;
        int64 new_tab = create_tab(0, TabRelation::LAST_CHILD, "about:blank");
        set_window_focused_tab(id, new_tab);
        claim_activity(app.ensure_activity_for_tab(new_tab));
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
        claim_activity(app.ensure_activity_for_tab(new_tab));
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
    case x31_hash("show_in_new_window"): {
        int64 tab = message[1];
        create_window(tab, tab);
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
        app.profile.register_as_browser();
        break;
    }
    case x31_hash("open_selected_links"): {
        if (activity) {
            activity->message_to_webview(json::array("open_selected_links"));
        }
        break;
    }
    case x31_hash("quit"): {
        app.quit();
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }
}

void Bark::focus_tab (int64 tab) {
    Transaction tr;
    unclose_tab(tab);
    set_window_focused_tab(id, tab);
     // Load this tab and, if traversing up or down, the next one
    claim_activity(app.ensure_activity_for_tab(tab));
     // Start the search from old_focused_tab because it could be closed
    if (old_focused_tab) {
        if (get_next_unclosed_tab(old_focused_tab) == tab) {
            if (int64 next = get_next_unclosed_tab(tab)) {
                app.ensure_activity_for_tab(next);
            }
        }
        else if (get_prev_unclosed_tab(old_focused_tab) == tab) {
            if (int64 prev = get_prev_unclosed_tab(tab)) {
                app.ensure_activity_for_tab(prev);
            }
        }
    }
}

void Bark::update (
    const vector<int64>& updated_tabs,
    const vector<int64>& updated_windows
) {
    send_update(updated_tabs);
}

void Bark::send_update (const std::vector<int64>& updated_tabs) {
    auto data = get_window_data(id);

    json::Array updates;
    updates.reserve(updated_tabs.size());

    bool focused_tab_changed = data->focused_tab != old_focused_tab;
    old_focused_tab = data->focused_tab;

    for (int64 tab : updated_tabs) {
        if (!tab) continue;
        auto t = get_tab_data(tab);
        int64 grandparent = t->parent ? get_tab_data(t->parent)->parent : 0;

         // Only send tab if it is a known tab (child or grandchild of expanded tab)
        if (!expanded_tabs.count(tab)
         && !expanded_tabs.count(t->parent)
         && !expanded_tabs.count(grandparent)
        ) {
            continue;
        }

         // Sending no tab data tells webview to delete tab
        if (t->deleted) {
            updates.emplace_back(json::array(tab));
            continue;
        }

        Activity* activity = app.activity_for_tab(tab);
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
        Str title = get_tab_data(data->focused_tab)->title;
        os_window.set_title(title.empty() ? "Sequoia" : (title + " – Sequoia").c_str());
        if (auto activity = app.activity_for_tab(data->focused_tab)) {
            if (activity->controller) {
                activity->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
            }
        }
        else {
            controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
    }

    if (!app.activity_for_tab(data->focused_tab)) leave_fullscreen();

    message_to_shell(json::array(
        "update",
        data->root_tab,
        data->focused_tab,
        updates
    ));
}

void Bark::message_to_shell (json::Value&& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_shell", s);
    webview->PostWebMessageAsJson(to_utf16(s).c_str());
}

} // namespace win32app
