#include "activity.h"

#include <stdexcept>
#include <wrl.h>

#include "../model/page.h"
#include "../model/transaction.h"
#include "../util/assert.h"
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

Activity::Activity (App* a, model::PageID p) : app(a), page(p) {
    LOG("new Activity"sv, this, page);

    new_webview([this](ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd){
        controller = wvc;
        webview = wv;
        webview_hwnd = hwnd;

        AH(webview->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
        {
            set_state(page, model::PageState::LOADING);
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
                set_title(page, from_utf16(title.get()));
            }
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_SourceChanged(
            Callback<ICoreWebView2SourceChangedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2SourceChangedEventArgs* args) -> HRESULT
        {
             // TODO: Make new page instead of changing this one!
             // WebView2 tends to have spurious navigations to about:blank, so don't
             // save the url in that case.
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            if (source.get() != L""sv && source.get() != L"about:blank"sv) {
                auto page_data = load(page);
                current_url = from_utf16(source.get());
                if (page_data->url != current_url) {
                    set_url(page, current_url);
                }
            }

            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
        {
            set_state(page, model::PageState::LOADED);
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
            create_last_child(page, from_utf16(url.get()));
            args->put_Handled(TRUE);
            return S_OK;
        }).Get(), nullptr));

        static std::wstring injection = to_utf16(slurp(exe_relative("res/injection.js"sv)));
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
            Window* window = app->window_for_page(page);
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
            auto page_data = load(page);
            if (page_data->viewing_view) {
                set_fullscreen(page_data->viewing_view, is_fullscreen());
            }
            return S_OK;
        }).Get(), nullptr));

        if (Window* window = app->window_for_page(page)) {
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
        set_favicon_url(page, message[1]);
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
//                link.move_before(page);
            }
            else if (alt) {
//                link.move_after(page);
            }
            else if (shift) {
                create_first_child(page, url, title);
            }
            else {
                create_last_child(page, url, title);
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
        throw Error("Unknown message name"sv);
    }
    }
}

void Activity::page_updated () {
    auto page_data = load(page);
    if (page_data->url != current_url) {
        navigate(page_data->url);
    }
}

bool Activity::navigate_url (Str url) {
    LOG("navigate_url"sv, page, url);
    auto hr = webview->Navigate(to_utf16(url).c_str());
    if (SUCCEEDED(hr)) {
        current_url = url;
        return true;
    }
    if (hr != E_INVALIDARG) AH(hr);
    return false;
}

void Activity::navigate_search (Str search) {
    LOG("navigate_search"sv, page, search);
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
        set_url(page, current_url = address);
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
