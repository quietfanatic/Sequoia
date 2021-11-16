#include "activities.h"

#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "model/actions.h"
#include "nursery.h"
#include "util/assert.h"
#include "util/files.h"
#include "util/hash.h"
#include "util/json.h"
#include "util/log.h"
#include "util/text.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

static unordered_map<model::PageID, Activity*> activities_by_page;

// TODO: ActivityUpdater

Activity::Activity (model::PageID p) : page(p) {
    LOG("new Activity", this, page);
    AA(!activities_by_page.contains(page));
    activities_by_page.emplace(page, this);

    new_webview([this](ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd){
        controller = wvc;
        webview = wv;
        webview_hwnd = hwnd;
        wil::com_ptr<ICoreWebView2Settings> settings;

        claimed_by_window(window);
        if (window) {
            window->resize();
             // TODO: check if this is necessary
            if (page->url != "about:blank") {
                AH(controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC));
            }
        }

        AH(webview->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
        {
            currently_loading = true;
            page->updated();
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
            if (wcscmp(source.get(), L"about:blank") != 0) {
                wil::unique_cotaskmem_string title;
                webview->get_DocumentTitle(&title);
                page->change_title(from_utf16(title.get()));
            }
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_HistoryChanged(
            Callback<ICoreWebView2HistoryChangedEventHandler>(
                [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
            BOOL back;
            webview->get_CanGoBack(&back);
            can_go_back = back;

            BOOL forward;
            webview->get_CanGoForward(&forward);
            can_go_forward = forward;

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
            if (wcscmp(source.get(), L"") != 0
             && wcscmp(source.get(), L"about:blank") != 0
            ) {
                navigated_url = "";
                page->change_url(from_utf16(source.get()));
            }
            else {
                page->change_url(navigated_url);
            }

            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
        {
             // TODO: store this on page
            currently_loading = false;
            page->updated();
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
            model::open_as_last_child(page, from_utf16(url.get()));
            args->put_Handled(TRUE);
            return S_OK;
        }).Get(), nullptr));

        static std::wstring injection = to_utf16(slurp(exe_relative("res/injection.js")));

        AH(webview->AddScriptToExecuteOnDocumentCreated(injection.c_str(), nullptr));

        AH(webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](
                    ICoreWebView2* sender,
                    ICoreWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw;
            args->get_WebMessageAsJson(&raw);
            message_from_webview(json::parse(from_utf16(raw.get())));
            return S_OK;
        }).Get(), nullptr));

        AH(controller->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                [this](
                    ICoreWebView2Controller* sender,
                    ICoreWebView2AcceleratorKeyPressedEventArgs* args
                )
        {
            if (!window) return S_OK;
            return window->on_AcceleratorKeyPressed(sender, args);
        }).Get(), nullptr));

        AH(webview->add_ContainsFullScreenElementChanged(
            Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
                [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
            is_fullscreen() ? window->enter_fullscreen() : window->leave_fullscreen();
            return S_OK;
        }).Get(), nullptr));

        navigate_url_or_search(page->url);
         // TODO: only set this when focusing the page
        page->change_visited();
    });
}

void Activity::message_from_webview(json::Value&& message) {
    const string& command = message[0];

    switch (x31_hash(command)) {
    case x31_hash("favicon"): {
        page->change_favicon_url(message[1]);
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
                model::open_as_first_child(page, url, title);
            }
            else {
                model::open_as_last_child(page, url, title);
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
//            const string& url = child[0];
//            const string& title = child[1];
//            create_tab(tab, TabRelation::LAST_CHILD, url, title);
//        }
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }
}

void Activity::message_to_webview (json::Value&& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_webview", s);
    AH(webview->PostWebMessageAsJson(to_utf16(s).c_str()));
}

void Activity::claimed_by_window (Window* w) {
    window = w;
    if (controller) {
        if (window) {
            AH(controller->put_IsVisible(TRUE));
            SetParent(webview_hwnd, window->os_window.hwnd);
        }
        else {
            AH(controller->put_IsVisible(FALSE));
             // TODO: Don't suspend tab until DOMContentLoaded
            auto wv3 = webview.try_query<ICoreWebView2_3>();
            AA(!!wv3);
            if (wv3) {
                AH(wv3->TrySuspend(Callback<ICoreWebView2TrySuspendCompletedHandler>(
                    [this](HRESULT hr, BOOL success){
                        AH(hr);
                        LOG("Suspend page", page.id, success);
                        return S_OK;
                    }
                ).Get()));
            }
            SetParent(webview_hwnd, HWND_MESSAGE);
        }
    }
}

void Activity::resize (RECT bounds) {
    if (controller) controller->put_Bounds(bounds);
}

bool Activity::navigate_url (const string& address) {
    auto hr = webview->Navigate(to_utf16(address).c_str());
    if (SUCCEEDED(hr)) {
        navigated_url = address;
        return true;
    }
    if (hr != E_INVALIDARG) AH(hr);
    return false;
}
void Activity::navigate_search (const string& search) {
     // Escape URL characters
    string url = "https://duckduckgo.com/?q=" + escape_url(search);
     // If the search URL is invalid, treat it as a bug
    AA(navigate_url(url));
}

void Activity::navigate_url_or_search (const string& address) {
    LOG("navigate_url_or_search", address);
    if (webview) {
        if (navigate_url(address)) return;
        if (address.find(' ') != string::npos
            || address.find('.') == string::npos
        ) {
            navigate_search(address);
        }
        else {
            string url = "http://" + address;
            if (navigate_url(url)) return;
            navigate_search(address);
        }
    }
    else {
        page->change_url(address);
    }
}

bool Activity::is_fullscreen () {
    if (!webview) return false;
    BOOL fs; AH(webview->get_ContainsFullScreenElement(&fs));
    return fs;
}

void Activity::leave_fullscreen () {
    if (!is_fullscreen()) return;
    webview->ExecuteScript(to_utf16("document.exitFullscreen()").c_str(), nullptr);
}

Activity::~Activity () {
    LOG("delete Activity", this);
    activities_by_page.erase(page);
    if (window) window->activity = nullptr;
    if (controller) controller->Close();
    page->updated();
}

Activity* activity_for_page (model::PageID id) {
    auto iter = activities_by_page.find(id);
    if (iter == activities_by_page.end()) return nullptr;
    else return iter->second;
}

Activity* ensure_activity_for_page (model::PageID id) {
    auto iter = activities_by_page.find(id);
    if (iter == activities_by_page.end()) {
        iter = activities_by_page.emplace(id, new Activity(*id)).first;
    }
    return iter->second;
}
