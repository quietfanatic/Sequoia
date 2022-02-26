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
                tab_updates.emplace_back(make_tab_json(app.model, id, &tab));
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
            auto view_data = app.model/view;
            navigate_activity_for_tab(
                write(app.model), view, view_data->focused_tab,
                message[1]
            );
            break;
        }
         // Toolbar buttons
        case x31_hash("back"): {
             // Connecting directly to win32app::Activity, because
             // model::ActivityData doesn't have concept of history
             // TODO: add enum LoadingType to model::ActiviyData
            auto activity = app.activity_for_view(view);
            if (activity && activity->webview) {
                activity->webview->GoBack();
            }
            break;
        }
        case x31_hash("forward"): {
            auto activity = app.activity_for_view(view);
            if (activity && activity->webview) {
                activity->webview->GoForward();
            }
            break;
        }
        case x31_hash("reload"): {
            auto activity_id = get_activity_for_view(app.model, view);
            if (activity_id) {
                reload(write(app.model), activity_id);
            }
            break;
        }
        case x31_hash("stop"): {
            auto activity_id = get_activity_for_view(app.model, view);
            if (activity_id) {
                finished_loading(write(app.model), activity_id);
            }
            break;
        }
        case x31_hash("investigate_error"): {
            webview->OpenDevToolsWindow();
            break;
        }
         // Tab actions
        case x31_hash("focus_tab"): {
            auto tab = model::EdgeID{message[1]};
            auto w = write(app.model);
            focus_tab(w, view, tab);
            focus_activity_for_tab(w, view, tab);
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
            // TODO
//            if (Activity* activity = app.activity_for_node(
//                focused_node(app.model, view)
//            )) {
//                activity->message_to_webview(
//                    json::array("open_selected_links"sv)
//                );
//            }
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
    if (ready) {
         // Generate new tab collection
        TabTree old_tabs = move(current_tabs);
        current_tabs = create_tab_tree(app, view);
         // Send changed tabs to shell
         // TODO: do less when tree structure hasn't changed?
        TabChanges changes = get_changed_tabs(update, old_tabs, current_tabs);
        json::Array tab_updates;
        tab_updates.reserve(changes.size());
        for (auto& [id, tab] : changes) {
            tab_updates.emplace_back(make_tab_json(
                app.model, id, tab ? &*tab : nullptr
            ));
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
