#include "activities.h"

#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "assert.h"
#include "data.h"
#include "hash.h"
#include "json/json.h"
#include "logging.h"
#include "nursery.h"
#include "utf8.h"
#include "util.h"
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
        if (window) window->resize();

        IWebView2Settings* settings;
        AH(webview->get_Settings(&settings));
        AH(settings->put_IsStatusBarEnabled(FALSE));

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
                set_tab_title(tab, to_utf8(title.get()));
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
                set_tab_url(tab, to_utf8(source.get()));
            }

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
            create_webpage_tab(tab, to_utf8(url.get()));
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
            message_from_webview(json::parse(to_utf8(raw.get())));
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_ContainsFullScreenElementChanged(
            Callback<IWebView2ContainsFullScreenElementChangedEventHandler>(
                [this](IWebView2WebView5* sender, IUnknown* args) -> HRESULT
        {
            BOOL fs;
            AH(webview->get_ContainsFullScreenElement(&fs));
            window->os_window.set_fullscreen(fs);
            return S_OK;
        }).Get(), nullptr));

        navigate_url_or_search(get_tab_url(tab));
    });
}

void Activity::message_from_webview(json::Value&& message) {
    const string& command = message[0];

    switch (x31_hash(command.c_str())) {
    case x31_hash("new_child_tab"): {
        const string& url = message[1];
        const string& title = message[2];
        create_webpage_tab(tab, url, title);
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
    string url = "https://duckduckgo.com/?q=";
    for (auto c : search) {
        switch (c) {
        case ' ': url += '+'; break;
        case '+':
        case '&':
        case ';':
        case '=':
        case '?':
        case '#': {
            stringstream ss;
            ss << "%" << hex << setw(2) << uint(c);
            url += ss.str();
            break;
        }
        default: url += c; break;
        }
    }
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
