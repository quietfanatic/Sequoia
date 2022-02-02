#include "activity.h"

#include <stdexcept>
#include <wrl.h>

#include "../model/link.h"
#include "../model/node.h"
#include "../model/view.h"
#include "../model/write.h"
#include "../util/error.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "app.h"
#include "nursery.h"
#include "window.h"

using namespace Microsoft::WRL;
using namespace std;

namespace win32app {

Activity::Activity (App& a, model::NodeID p) : app(a), node(p) {
    LOG("new Activity"sv, this, node);

    app.nursery.new_webview([this](
        ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd
    ) {
        controller = wvc;
        webview = wv;
        webview_hwnd = hwnd;

        AH(webview->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
        {
            set_state(write(app.model), node, model::NodeState::LOADING);
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_DocumentTitleChanged(
            Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
             // WebView2 tends to have spurious navigations to about:blank, so don't
             // save the title in that case.
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            if (source.get() == L"about:blank"sv) {
                wil::unique_cotaskmem_string title;
                webview->get_DocumentTitle(&title);
                set_title(write(app.model), node, from_utf16(title.get()));
            }
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_SourceChanged(
            Callback<ICoreWebView2SourceChangedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2SourceChangedEventArgs* args) -> HRESULT
        {
             // TODO: Make new node instead of changing this one!
             // WebView2 tends to have spurious navigations to about:blank, so don't
             // save the url in that case.
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            if (source.get() != L""sv && source.get() != L"about:blank"sv) {
                current_url = from_utf16(source.get());
                if ((app.model/node)->url != current_url) {
                    set_url(write(app.model), node, current_url);
                }
            }

            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
        {
            set_state(write(app.model), node, model::NodeState::LOADED);
            return S_OK;
        }).Get(), nullptr));

         // This doesn't work for middle-clicking links
         // TODO: How true is that now?  Investigate when this is called.
        AH(webview->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
        {
            wil::unique_cotaskmem_string url;
            args->get_Uri(&url);
            create_last_child(write(app.model), node, from_utf16(url.get()));
            args->put_Handled(TRUE);
            return S_OK;
        }).Get(), nullptr));

        static std::wstring injection = to_utf16(
            slurp(exe_relative("res/win32app/activity.js"sv))
        );
        AH(webview->AddScriptToExecuteOnDocumentCreated(injection.c_str(), nullptr));

        AH(webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](
                    ICoreWebView2* sender,
                    ICoreWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw16;
            args->get_WebMessageAsJson(&raw16);
            string raw = from_utf16(raw16.get());
            LOG("Activity::message_from_webview"sv, raw);
            json::Value message = json::parse(raw);
            message_from_webview(message);
            return S_OK;
        }).Get(), nullptr));

        AH(controller->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                [this](
                    ICoreWebView2Controller* sender,
                    ICoreWebView2AcceleratorKeyPressedEventArgs* args
                )
        {
            Window* window = app.window_for_node(node);
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

        AH(webview->add_ContainsFullScreenElementChanged(
            Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
                [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
            if (auto view = (app.model/node)->viewing_view) {
                set_fullscreen(write(app.model), view, is_fullscreen());
            }
            return S_OK;
        }).Get(), nullptr));

        if (Window* window = app.window_for_node(node)) {
            window->reflow();
        }
    });
}

void Activity::message_to_webview (const json::Value& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_webview"sv, s);
    AH(webview->PostWebMessageAsJson(to_utf16(s).c_str()));
}

void Activity::message_from_webview (const json::Value& message) {
    Str command = message[0];

    switch (x31_hash(command)) {
        case x31_hash("favicon"): {
            set_favicon_url(write(app.model), node, message[1]);
            break;
        }
        case x31_hash("click_link"): {
            std::string url = message[1];
            std::string title = message[2];
            int button = message[3];
            bool double_click = message[4];
            bool shift = message[5];
            bool alt = message[6];
            bool ctrl = message[7];
            if (button == 1) {
    //            if (double_click) {
    //                if (last_created_new_child && window) {
    //                     // TODO: figure out why the new tab doesn't get loaded
    //                    set_window_focused_tab(window->id, last_created_new_child);
    //                }
    //            }
                if (alt && shift) {
                    // TODO: get this activity's link id from view
    //                link.move_before(node);
                }
                else if (alt) {
    //                link.move_after(node);
                }
                else if (shift) {
                    create_first_child(write(app.model), node, url, title);
                }
                else {
                    create_last_child(write(app.model), node, url, title);
                }
            }
            break;
        }
        case x31_hash("new_children"): {
         // TODO
    //        last_created_new_child = 0;
    //        const json::Array& children = message[1];
    //        Transaction tr;
    //        for (auto& child : children) {
    //            Str url = child[0];
    //            Str title = child[1];
    //            create_tab(tab, TabRelation::LAST_CHILD, url, title);
    //        }
            break;
        }
        default: {
            ERR("Unknown message name"sv);
        }
    }
}

void Activity::node_updated () {
    auto url = (app.model/node)->url;
    if (url != current_url) navigate(url);
}

bool Activity::navigate_url (Str url) {
    LOG("navigate_url"sv, node, url);
    auto hr = webview->Navigate(to_utf16(url).c_str());
    if (SUCCEEDED(hr)) {
        current_url = url;
        return true;
    }
    if (hr != E_INVALIDARG) AH(hr);
    return false;
}

void Activity::navigate_search (Str search) {
    LOG("navigate_search"sv, node, search);
     // Escape URL characters
    String url = "https://duckduckgo.com/?q="sv + escape_url(search);
     // If the search URL is invalid, treat it as a bug
    AA(navigate_url(url));
}

void Activity::navigate (Str address) {
    if (webview) {
        if (navigate_url(address)) return;
        if (address.find(' ') != string::npos
            || address.find('.') == string::npos
        ) {
            navigate_search(address);
        }
        else {
            String url = "http://"sv + address;
            if (navigate_url(url)) return;
            navigate_search(address);
        }
    }
    else {
        set_url(write(app.model), node, current_url = address);
    }
}

bool Activity::is_fullscreen () {
    if (!webview) return false;
    BOOL fs; AH(webview->get_ContainsFullScreenElement(&fs));
    return fs;
}

void Activity::leave_fullscreen () {
    if (!is_fullscreen()) return;
    webview->ExecuteScript(to_utf16("document.exitFullscreen()"sv).c_str(), nullptr);
}

Activity::~Activity () {
    LOG("delete Activity"sv, this);
    if (controller) controller->Close();
}

} // namespace win32app
