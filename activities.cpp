#include "activities.h"

#include <fstream>
#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "assert.h"
#include "hash.h"
#include "json/json.h"
#include "tabs.h"
#include "main.h"
#include "Window.h"
#include "windows_utf8.h"

using namespace Microsoft::WRL;
using namespace std;

std::set<Activity*> all_activities;


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

Activity::Activity (Tab* t) : tab(t) {
    A(!tab->activity);
    tab->activity = this;

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
            wil::unique_cotaskmem_string title;
            webview->get_DocumentTitle(&title);
            tab->set_title(to_utf8(title.get()));
            Tab::commit();
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

            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            tab->set_url(to_utf8(source.get()));
            Tab::commit();

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
            Tab::open_webpage(tab->id, to_utf8(url.get()));
            args->put_Handled(TRUE);

            return S_OK;
        }).Get(), &token));

        static std::wstring injection = []{
            wifstream file (exe_relative("res/injection.js"), ios::ate);
            auto len = file.tellg();
            file.seekg(0, ios::beg);
            std::wstring r (len, 0);
            file.read(const_cast<wchar_t*>(r.data()), len);
            return r;
        }();

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

        webview->Navigate(to_utf16(tab->url).c_str());

        return S_OK;
    }).Get()));

    all_activities.insert(this);
}

void Activity::message_from_webview(json::Value&& message) {
    const string& command = message[0];

    switch (x31_hash(command.c_str())) {
    case x31_hash("new_child_tab"): {
        const string& url = message[1];
        const string& title = message[2];
        Tab::open_webpage(tab->id, url, title);
        Tab::commit();
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
    all_activities.erase(this);
    tab->activity = nullptr;
    if (window) window->activity = nullptr;
    webview->Close();
}

