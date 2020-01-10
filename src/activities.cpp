#include "activities.h"

#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "data.h"
#include "nursery.h"
#include "util/assert.h"
#include "util/files.h"
#include "util/hash.h"
#include "util/json.h"
#include "util/logging.h"
#include "util/text.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

static map<int64, Activity*> activities_by_tab;

Activity::Activity (int64 t) : tab(t) {
    LOG("new Activity", this);
    A(!activities_by_tab.contains(t));
    activities_by_tab.emplace(t, this);

    new_webview([this](WebView* wv, HWND hwnd){
        webview = wv;
        webview_hwnd = hwnd;

        claimed_by_window(window);
        if (window) {
            window->resize();
            if (get_tab_data(tab)->url != "about:blank") {
                AH(webview->MoveFocus(WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC));
            }
        }

        AH(webview->add_NavigationStarting(
            Callback<IWebView2NavigationStartingEventHandler>([this](
                IWebView2WebView* sender,
                IWebView2NavigationStartingEventArgs* args) -> HRESULT
        {
            currently_loading = true;
            if (window) window->send_activity();
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_DocumentTitleChanged(
            Callback<IWebView2DocumentTitleChangedEventHandler>(
                [this](IWebView2WebView* sender, IUnknown* args) -> HRESULT
        {
             // See below regarding about:blank
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            if (wcscmp(source.get(), L"about:blank") != 0) {
                wil::unique_cotaskmem_string title;
                webview->get_DocumentTitle(&title);
                set_tab_title(tab, from_utf16(title.get()));
            }
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_DocumentStateChanged(
            Callback<IWebView2DocumentStateChangedEventHandler>([this](
                IWebView2WebView* sender,
                IWebView2DocumentStateChangedEventArgs* args) -> HRESULT
        {
            BOOL back;
            webview->get_CanGoBack(&back);
            can_go_back = back;

            BOOL forward;
            webview->get_CanGoForward(&forward);
            can_go_forward = forward;

             // WebView2 tends to have spurious navigations to about:blank, so don't
             // save the url in that case.
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            if (wcscmp(source.get(), L"about:blank") != 0) {
                set_tab_url(tab, from_utf16(source.get()));
            }

            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_NavigationCompleted(
            Callback<IWebView2NavigationCompletedEventHandler>([this](
                IWebView2WebView* sender,
                IWebView2NavigationCompletedEventArgs* args) -> HRESULT
        {
            currently_loading = false;
            if (window) window->send_activity();
            return S_OK;
        }).Get(), nullptr));

         // This doesn't work for middle-clicking links yet
        AH(webview->add_NewWindowRequested(
            Callback<IWebView2NewWindowRequestedEventHandler>([this](
                IWebView2WebView* sender,
                IWebView2NewWindowRequestedEventArgs* args) -> HRESULT
        {
            wil::unique_cotaskmem_string url;
            args->get_Uri(&url);
            create_tab(tab, TabRelation::LAST_CHILD, from_utf16(url.get()));
            args->put_Handled(TRUE);
            return S_OK;
        }).Get(), nullptr));

        static std::wstring injection = to_utf16(slurp(exe_relative("res/injection.js")));

        AH(webview->AddScriptToExecuteOnDocumentCreated(injection.c_str(), nullptr));

        AH(webview->add_WebMessageReceived(
            Callback<IWebView2WebMessageReceivedEventHandler>(
                [this](
                    IWebView2WebView* sender,
                    IWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw;
            args->get_WebMessageAsJson(&raw);
            message_from_webview(json::parse(from_utf16(raw.get())));
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_AcceleratorKeyPressed(
            Callback<IWebView2AcceleratorKeyPressedEventHandler>(
                [this](
                    IWebView2WebView* sender,
                    IWebView2AcceleratorKeyPressedEventArgs* args
                )
        {
            if (!window) return S_OK;
            return window->on_AcceleratorKeyPressed(sender, args);
        }).Get(), nullptr));

        AH(webview->add_ContainsFullScreenElementChanged(
            Callback<IWebView2ContainsFullScreenElementChangedEventHandler>(
                [this](IWebView2WebView5* sender, IUnknown* args) -> HRESULT
        {
            window->os_window.set_fullscreen(is_fullscreen());
            return S_OK;
        }).Get(), nullptr));

        navigate_url_or_search(get_tab_data(tab)->url);
    });
}

void Activity::message_from_webview(json::Value&& message) {
    const string& command = message[0];

    switch (x31_hash(command.c_str())) {
    case x31_hash("favicon"): {
        const string& favicon = message[1];
        set_tab_favicon(tab, favicon);
        break;
    }
    case x31_hash("new_child_tab"): {
        const string& url = message[1];
        const string& title = message[2];
        last_created_new_child = create_tab(tab, TabRelation::LAST_CHILD, url, title);
        break;
    }
    case x31_hash("switch_to_new_child"): {
        if (last_created_new_child && window) {
            set_window_focused_tab(window->id, last_created_new_child);
        }
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }
}

void Activity::claimed_by_window (Window* w) {
    window = w;
    if (webview) {
        if (window) {
            AH(webview->put_IsVisible(TRUE));
            SetParent(webview_hwnd, window->os_window.hwnd);
        }
        else {
            AH(webview->put_IsVisible(FALSE));
            SetParent(webview_hwnd, HWND_MESSAGE);
        }
    }
}

void Activity::resize (RECT bounds) {
    if (webview) {
        webview->put_Bounds(bounds);
    }
}

bool Activity::navigate_url (const string& address) {
    auto hr = webview->Navigate(to_utf16(address).c_str());
    if (SUCCEEDED(hr)) return true;
    if (hr != E_INVALIDARG) AH(hr);
    return false;
}
void Activity::navigate_search (const string& search) {
     // Escape URL characters
    string url = "https://duckduckgo.com/?q=" + escape_url(search);
     // If the search URL is invalid, treat it as a bug
    A(navigate_url(url));
}

void Activity::navigate_url_or_search (const string& address) {
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
        set_tab_url(tab, address);
    }
}

bool Activity::is_fullscreen () {
    if (!webview) return false;
    BOOL fs; AH(webview->get_ContainsFullScreenElement(&fs));
    return fs;
}

void Activity::enter_fullscreen () {
    if (!webview) return;
    webview->ExecuteScript(to_utf16("document.documentElement.requestFullscreen()").c_str(), nullptr);
}

void Activity::leave_fullscreen () {
    if (!webview) return;
    webview->ExecuteScript(to_utf16("document.exitFullscreen()").c_str(), nullptr);
}
void Activity::toggle_fullscreen () {
    is_fullscreen() ? leave_fullscreen() : enter_fullscreen();
}

Activity::~Activity () {
    LOG("delete Activity", this);
    activities_by_tab.erase(tab);
    if (window) window->activity = nullptr;
    webview->Close();
}

Activity* activity_for_tab (int64 id) {
    auto iter = activities_by_tab.find(id);
    if (iter == activities_by_tab.end()) return nullptr;
    else return iter->second;
}

Activity* ensure_activity_for_tab (int64 id) {
    auto iter = activities_by_tab.find(id);
    if (iter == activities_by_tab.end()) {
        iter = activities_by_tab.emplace(id, new Activity(id)).first;
    }
    return iter->second;
}
