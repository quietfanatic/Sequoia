#include "activities.h"

#include <fstream>
#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "assert.h"
#include "data.h"
#include "hash.h"
#include "json/json.h"
#include "main.h"
#include "utf8.h"
#include "util.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

static map<int64, Activity*> activities_by_tab;

static HWND get_nursery () {
    static HWND nursery = []{
        static auto class_name = "Sequoia Nursery";
        static bool init = []{
            WNDCLASSEX c {};
            c.cbSize = sizeof(WNDCLASSEX);
            c.lpfnWndProc = DefWindowProc;
            c.hInstance = GetModuleHandle(nullptr);
            c.lpszClassName = class_name;
            AW(RegisterClassEx(&c));
            return true;
        }();
        HWND hwnd = CreateWindow(
            class_name,
            "Sequoia Nursery",
            0,
            0, 0,
            0, 0,
            HWND_MESSAGE,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );
        AW(hwnd);
        return hwnd;
    }();
    return nursery;
}

Activity::Activity (int64 t) : tab(t) {
    A(!activities_by_tab.contains(t));
    activities_by_tab.emplace(t, this);

    AH(webview_environment->CreateWebView(get_nursery(),
        Callback<IWebView2CreateWebViewCompletedHandler>(
            [this](HRESULT hr, IWebView2WebView* wv) -> HRESULT
    {
        AH(hr);
        AH(wv->QueryInterface(IID_PPV_ARGS(&webview)));
        AW(webview_hwnd = GetWindow(get_nursery(), GW_CHILD));

        claimed_by_window(window);
        if (window) window->resize_everything();

        EventRegistrationToken token;
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
        }).Get(), &token));

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
        }).Get(), &token));

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
        }).Get(), &token));

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
        }).Get(), &token));

        webview->Navigate(to_utf16(get_tab_url(tab)).c_str());

        return S_OK;
    }).Get()));
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
            SetParent(webview_hwnd, window->hwnd);
        }
        else {
            SetParent(webview_hwnd, HWND_MESSAGE);
            AH(webview->put_IsVisible(FALSE));
        }
    }
}

void Activity::resize (RECT bounds) {
    if (webview) {
        webview->put_Bounds(bounds);
    }
}

Activity::~Activity () {
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
