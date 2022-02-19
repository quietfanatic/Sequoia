#include "shell.h"

#include <windows.h>
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
#include "app.h"
#include "nursery.h"
#include "profile.h"
#include "window.h"

using namespace Microsoft::WRL;
using namespace std;

namespace win32app {

Shell::Shell (App& a, model::ViewID v) : app(a), view(v) {
     // TODO: Fix possible use-after-free of this
    app.nursery.new_webview([this](
        ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd
    ){
        controller = wvc;
        webview = wv;
        webview_hwnd = hwnd;

        webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](
                    ICoreWebView2* sender,
                    ICoreWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw16;
            args->get_WebMessageAsJson(&raw16);
            string raw = from_utf16(raw16.get());
            LOG("Shell::message_from_webview"sv, raw);
            message_from_webview(json::parse(raw));
            return S_OK;
        }).Get(), nullptr);

        AH(controller->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                [this](
                    ICoreWebView2Controller* sender,
                    ICoreWebView2AcceleratorKeyPressedEventArgs* args
                )
        {
            Window* window = app.window_for_view(view);
            if (!window) return S_OK;
            COREWEBVIEW2_KEY_EVENT_KIND kind;
            AH(args->get_KeyEventKind(&kind));
            switch (kind) {
                case COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN: break;
                case COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN: break;
                default: return S_OK;
            }
            UINT key; AH(args->get_VirtualKey(&key));
            auto handle = window->get_key_handler(
                key,
                GetKeyState(VK_SHIFT) < 0,
                GetKeyState(VK_CONTROL) < 0,
                GetKeyState(VK_MENU) < 0
            );
            if (handle) {
                AH(args->put_Handled(TRUE));
                INT l; AH(args->get_KeyEventLParam(&l));
                bool repeated = l & (1 << 30);
                if (!repeated) handle();
            }
            return S_OK;
        }).Get(), nullptr));

        webview->Navigate(to_utf16(
            exe_relative("res/win32app/shell.html"sv)
        ).c_str());

        if (Window* window = app.window_for_view(view)) {
            window->reflow();
        }
    });
};

Shell::~Shell () {
    if (controller) {
        AH(controller->Close());
    }
}

void Shell::select_location () {
    if (!ready) return;
    message_to_webview(json::array("select_location"sv));
    AH(controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC));
}

static json::Array make_tab_json (
    const model::Model& model,
    model::EdgeID edge, const Tab& tab
) {
    AA(edge);
    auto edge_data = model/edge;
    if (!edge_data) {
        return json::array(edge);
    }
    if (tab.node) {
        auto node_data = model/tab.node;
         // This code path will probably never be hit.
        string title = node_data->title;
        if (title.empty()) title = edge_data->title;
        if (title.empty()) title = node_data->url;

        return json::array(
            edge,
            tab.parent,
            edge_data->position.hex(),
            node_data->url,
            node_data->favicon_url,
            title,
            tab.flags
        );
    }
    else {
        return json::array(
            edge,
            tab.parent,
            edge_data->position.hex(),
            ""s, ""s, ""s,
            tab.flags
        );
    }
}

void Shell::message_from_webview (const json::Value& message) {
    try {

    const string& command = message[0];
    switch (x31_hash(command.c_str())) {
        case x31_hash("ready"): {
            message_to_webview(json::array(
                "settings"sv,
                json::object(
                    std::pair{"theme"sv, app.settings.theme}
                )
            ));
            current_tabs = create_tab_tree(app, view);
            json::Array tab_updates;
            tab_updates.reserve(current_tabs.size());
            for (auto& [id, tab] : current_tabs) {
                tab_updates.emplace_back(make_tab_json(app.model, id, tab));
            }
            message_to_webview(json::array("view"sv, tab_updates));
            ready = true;
            if (waiting_for_ready) {
                waiting_for_ready = false;
                app.quit();
            }
            break;
        }
        case x31_hash("resize"): {
             // TODO: set sidebar width or something
            if (Window* window = app.window_for_view(view)) {
                window->reflow();
            }
            break;
        }
        case x31_hash("navigate"): {
             // TODO: is this actually what we want to do?
            auto node = focused_node(app.model, view);
            if (node) {
                set_url(write(app.model), node, message[1]);
            }
            break;
        }
         // Toolbar buttons
        case x31_hash("back"): {
            Activity* activity = app.activity_for_node(
                focused_node(app.model, view)
            );
            if (activity && activity->webview) {
                activity->webview->GoBack();
            }
            break;
        }
        case x31_hash("forward"): {
            Activity* activity = app.activity_for_node(
                focused_node(app.model, view)
            );
            if (activity && activity->webview) {
                activity->webview->GoForward();
            }
            break;
        }
        case x31_hash("reload"): {
            Activity* activity = app.activity_for_node(
                focused_node(app.model, view)
            );
            if (activity && activity->webview) {
                activity->webview->Reload();
            }
            break;
        }
        case x31_hash("stop"): {
            Activity* activity = app.activity_for_node(
                focused_node(app.model, view)
            );
            if (activity && activity->webview) {
                activity->webview->Stop();
            }
            break;
        }
        case x31_hash("investigate_error"): {
            webview->OpenDevToolsWindow();
            break;
        }
         // Tab actions
        case x31_hash("focus_tab"): {
            focus_tab(write(app.model), view, model::EdgeID{message[1]});
            break;
        }
        case x31_hash("new_child"): {
            auto w = write(app.model);
            auto edge = make_last_child(w, focused_node(w, view), model::NodeID{});
            focus_tab(w, view, edge);
            break;
        }
        case x31_hash("trash_tab"): {
            trash_tab(write(app.model), view, model::EdgeID{message[1]});
            break;
        }
        case x31_hash("move_tab_before"): {
            model::EdgeID edge {message[1]};
            model::EdgeID target {message[2]};
            if (edge && target) {
                move_before(write(app.model), edge, target);
            }
            break;
        }
        case x31_hash("move_tab_after"): {
            model::EdgeID edge {message[1]};
            model::EdgeID target {message[2]};
            if (edge && target) {
                move_after(write(app.model), edge, target);
            }
            break;
        }
        case x31_hash("move_tab_first_child"): {
            model::EdgeID edge {message[1]};
            model::NodeID parent {message[2]};
            if (edge) {
                move_first_child(write(app.model), edge, parent);
            }
            break;
        }
        case x31_hash("move_tab_last_child"): {
            model::EdgeID edge {message[1]};
            model::NodeID parent {message[2]};
            if (edge) {
                move_last_child(write(app.model), edge, parent);
            }
            break;
        }
        case x31_hash("expand_tab"): {
            expand_tab(write(app.model), view, model::EdgeID{message[1]});
            break;
        }
        case x31_hash("contract_tab"): {
            contract_tab(write(app.model), view, model::EdgeID{message[1]});
            break;
        }
         // Main menu
        case x31_hash("fullscreen"): {
            bool currently_fullscreen = (app.model/view)->fullscreen;
            set_fullscreen(write(app.model), view, !currently_fullscreen);
            break;
        }
        case x31_hash("register_as_browser"): {
            app.profile.register_as_browser();
            break;
        }
        case x31_hash("open_selected_links"): {
            if (Activity* activity = app.activity_for_node(
                focused_node(app.model, view)
            )) {
                activity->message_to_webview(
                    json::array("open_selected_links"sv)
                );
            }
            break;
        }
        case x31_hash("quit"): {
            app.quit();
            break;
        }
        default: {
            ERR("Unknown message from shell: "sv + command);
        }
    }

    } catch (exception& e) {
        ERR("Uncaught exception: "sv + e.what());
    }
}

void Shell::update (const model::Update& update) {
     // Generate new tab collection
    TabTree old_tabs = move(current_tabs);
    current_tabs = create_tab_tree(app, view);
     // Send changed tabs to shell
    if (ready) {
        TabChanges changes = get_changed_tabs(update, old_tabs, current_tabs);
        json::Array tab_updates;
        tab_updates.reserve(changes.size());
        for (auto& [id, tab] : changes) {
            if (tab) {
                tab_updates.emplace_back(make_tab_json(app.model, id, *tab));
            }
            else {
                tab_updates.emplace_back(json::array(id));
            }
        }
        if (tab_updates.size()) {
            message_to_webview(json::array("update"sv, tab_updates));
        }
    }
}

void Shell::message_to_webview (const json::Value& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_webview"sv, s);
    AH(webview->PostWebMessageAsJson(to_utf16(s).c_str()));
}

void Shell::wait_for_ready () {
    if (ready) return;
    waiting_for_ready = true;
    app.run();
}

} // namespace win32app
