#include "shell.h"

#include <windows.h>
#include <wrl.h>

#include "../model/actions.h"
#include "../model/link.h"
#include "../model/page.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "activity.h"
#include "main.h"
#include "nursery.h"
#include "profile.h"
#include "window.h"

using namespace Microsoft::WRL;
using namespace std;

Shell::Shell (model::ViewID v) : view(v) {
     // TODO: Fix possible use-after-free of this
    new_webview([this](ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd){
        controller = wvc;
        webview = wv;
        webview_hwnd = hwnd;

        if (Window* window = window_for_view(view)) {
            window->resize();
        }
        webview->Navigate(to_utf16(exe_relative("res/shell.html")).c_str());

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
            LOG("Shell::message_from_webview", raw);
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
            Window* window = window_for_view(view);
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
    });
};

Shell::~Shell () {
    if (controller) {
        AH(controller->Close());
    }
}

void Shell::select_location () {
    if (!ready) return;
    message_to_webview(json::array("select_location"));
    AH(controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC));
}

static json::Array make_tab_json (
    const model::ViewData& view, model::LinkID link, const model::Tab& tab
) {
     // Sending just id means tab should be removed
    if (!tab.page) {
        return json::array(link);
    }
     // This code path will probably never be hit.
    if (link && !link->exists) {
        return json::array(link);
    }
     // Choose title
    string title = tab.page->title;
    if (title.empty() && link) title = link->title; // TODO: don't use this if inverted link
    if (title.empty()) title = tab.page->url;

    return json::array(
        link,
        link ? json::Value(tab.parent) : json::Value(nullptr),
        link ? link->position.hex() : "80",
        tab.page->url,
        tab.page->favicon_url,
        title,
        tab.flags
    );
}

void Shell::message_from_webview (const json::Value& message) {
    try {

    const string& command = message[0];
    switch (x31_hash(command.c_str())) {
    case x31_hash("ready"): {
        message_to_webview(json::array(
            "settings",
            json::Object{
                std::pair{"theme", settings::theme}
            }
        ));
        tabs = create_tab_tree(*view);
        json::Array tab_updates;
        tab_updates.reserve(tabs.size());
        for (auto& [id, tab] : tabs) {
            tab_updates.emplace_back(make_tab_json(*view, id, tab));
        }
        message_to_webview(json::array("view", tab_updates));
        ready = true;
        break;
    }
    case x31_hash("resize"): {
         // TODO: set sidebar width or something
        if (Window* window = window_for_view(view)) {
            window->resize();
        }
        break;
    }
    case x31_hash("navigate"): {
        model::navigate_focused_page(view, message[1]);
        break;
    }
     // Toolbar buttons
    case x31_hash("back"): {
        Activity* activity = activity_for_page(view->focused_page());
        if (activity && activity->webview) {
            activity->webview->GoBack();
        }
        break;
    }
    case x31_hash("forward"): {
        Activity* activity = activity_for_page(view->focused_page());
        if (activity && activity->webview) {
            activity->webview->GoForward();
        }
        break;
    }
    case x31_hash("reload"): {
        Activity* activity = activity_for_page(view->focused_page());
        if (activity && activity->webview) {
            activity->webview->Reload();
        }
        break;
    }
    case x31_hash("stop"): {
        Activity* activity = activity_for_page(view->focused_page());
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
        model::focus_tab(view, model::LinkID{message[1]});
        break;
    }
    case x31_hash("new_child"): {
        model::open_as_last_child_in_view(view, model::LinkID{message[1]}, "about:blank");
        break;
    }
    case x31_hash("trash_tab"): {
        model::trash_tab(view, model::LinkID{message[1]});
        break;
    }
    case x31_hash("delete_tab"): {
        model::delete_tab(view, model::LinkID{message[1]});
        break;
    }
    case x31_hash("move_tab_before"): {
        model::LinkID link {message[1]};
        model::LinkID target {message[2]};
        if (link && target) {
            model::move_link_before(link, target);
        }
        break;
    }
    case x31_hash("move_tab_after"): {
        model::LinkID link {message[1]};
        model::LinkID target {message[2]};
        if (link && target) {
            model::move_link_after(link, target);
        }
        break;
    }
    case x31_hash("move_tab_first_child"): {
        model::LinkID link {message[1]};
        model::PageID parent {message[2]};
        if (link) {
            model::move_link_first_child(link, parent);
        }
        break;
    }
    case x31_hash("move_tab_last_child"): {
        model::LinkID link {message[1]};
        model::PageID parent {message[2]};
        if (link) {
            model::move_link_last_child(link, parent);
        }
        break;
    }
    case x31_hash("expand_tab"): {
        model::expand_tab(view, model::LinkID{message[1]});
        break;
    }
    case x31_hash("contract_tab"): {
        model::contract_tab(view, model::LinkID{message[1]});
        break;
    }
     // Main menu
    case x31_hash("fullscreen"): {
        model::change_view_fullscreen(view, !view->fullscreen);
        break;
    }
    case x31_hash("register_as_browser"): {
        register_as_browser();
        break;
    }
    case x31_hash("open_selected_links"): {
        if (Activity* activity = activity_for_page(view->focused_page())) {
            activity->message_to_webview(json::array("open_selected_links"));
        }
        break;
    }
    case x31_hash("quit"): {
        quit();
        break;
    }
    default: {
        throw Error("Unknown message from shell: " + command);
    }
    }

    } catch (exception& e) {
        show_string_error(__FILE__, __LINE__, (string("Uncaught exception: ") + e.what()).c_str());
        throw;
    }
}

void Shell::update (const model::Update& update) {
     // Generate new tab collection
    model::TabTree old_tabs = move(tabs);
    tabs = create_tab_tree(*view);
     // Send changed tabs to shell
    if (ready) {
        model::TabChanges changes = get_changed_tabs(old_tabs, tabs, update);
        json::Array tab_updates;
        tab_updates.reserve(changes.size());
        for (auto& [id, tab] : changes) {
            tab_updates.emplace_back(make_tab_json(*view, id, tab));
        }
        if (tab_updates.size()) {
            message_to_webview(json::array("update", tab_updates));
        }
    }
}

void Shell::message_to_webview (const json::Value& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_webview", s);
    AH(webview->PostWebMessageAsJson(to_utf16(s).c_str()));
}


///// Shell registry

static unordered_map<model::ViewID, Shell*> shells_by_view;

struct ShellRegistry : model::Observer {
    void Observer_after_commit (
        const model::Update& update
    ) override {
        for (model::ViewID view : update.views) {
            if (view->exists && !view->closed_at) {
                Shell*& shell = shells_by_view[view];
                if (shell) shell->update(update);
                else shell = new Shell (view);
            }
            else {
                auto iter = shells_by_view.find(view);
                if (iter != shells_by_view.end()) {
                    delete iter->second;
                    shells_by_view.erase(iter);
                }
            }
        }
    }
};
static ShellRegistry shell_registry;

Shell* shell_for_view (model::ViewID view) {
    auto iter = shells_by_view.find(view);
    if (iter != shells_by_view.end()) return iter->second;
    else return nullptr;
}
