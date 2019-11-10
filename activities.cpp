#include "activities.h"

#include <WebView2.h>
#include <wrl.h>

#include "assert.h"
#include "main.h"
#include "Window.h"

using namespace Microsoft::WRL;

std::set<Activity*> all_activities;

Activity::Activity () {
    all_activities.insert(this);

    static HWND nursery = []{
        static auto class_name = L"Sequoia Nursery";
        static bool init = []{
            WNDCLASSEX c {};
            c.cbSize = sizeof(WNDCLASSEX);
            c.lpfnWndProc = DefWindowProc;
            c.hInstance = GetModuleHandle(nullptr);
            c.lpszClassName = class_name;
            ASSERT(RegisterClassEx(&c));
            return true;
        }();
        HWND hwnd = CreateWindow(
            class_name,
            L"Sequoia Nursery",
            0,
            0, 0,
            0, 0,
            HWND_MESSAGE,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );
        ASSERT(hwnd);
        return hwnd;
    }();

    ASSERT_HR(webview_environment->CreateWebView(nursery,
        Callback<IWebView2CreateWebViewCompletedHandler>(
            [this](HRESULT hr, IWebView2WebView* wv) -> HRESULT
    {
        ASSERT_HR(hr);
        wv->QueryInterface(IID_PPV_ARGS(&webview));
        ASSERT(webview_hwnd = GetWindow(nursery, GW_CHILD));
        ASSERT(SetParent(webview_hwnd, HWND_MESSAGE));

        webview->Navigate(L"https://duckduckgo.com/");
        if (window) window->resize_everything();

        EventRegistrationToken token;
        ASSERT_HR(webview->add_DocumentTitleChanged(
            Callback<IWebView2DocumentTitleChangedEventHandler>(
                [this](IWebView2WebView* sender, IUnknown* args) -> HRESULT
        {
            wil::unique_cotaskmem_string title;
            webview->get_DocumentTitle(&title);
            if (window) window->set_title(title.get());
            return S_OK;
        }).Get(), &token));
        ASSERT_HR(webview->add_DocumentStateChanged(
            Callback<IWebView2DocumentStateChangedEventHandler>([this](
                IWebView2WebView* sender,
                IWebView2DocumentStateChangedEventArgs* args) -> HRESULT
        {
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            url = source.get();
            BOOL back;
            webview->get_CanGoBack(&back);
            can_go_back = back;
            BOOL forward;
            webview->get_CanGoForward(&forward);
            can_go_forward = forward;
            if (window) window->update_shell();
            return S_OK;
        }).Get(), &token));

        return S_OK;
    }).Get()));
}

Activity::~Activity() {
    all_activities.erase(this);
    if (window) window->set_activity(nullptr);
    webview->Close();
}

