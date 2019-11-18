#include "activities.h"

#include <WebView2.h>
#include <wrl.h>

#include "assert.h"
#include "tabs.h"
#include "main.h"
#include "Window.h"

using namespace Microsoft::WRL;

std::set<Activity*> all_activities;


static HWND get_nursery () {
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
    return nursery;
}

Activity::Activity (Tab* t) : tab(t) {
    ASSERT(!tab->activity);
    tab->activity = this;

    ASSERT_HR(webview_environment->CreateWebView(get_nursery(),
        Callback<IWebView2CreateWebViewCompletedHandler>(
            [this](HRESULT hr, IWebView2WebView* wv) -> HRESULT
    {
        ASSERT_HR(hr);
        ASSERT_HR(wv->QueryInterface(IID_PPV_ARGS(&webview)));
        ASSERT(webview_hwnd = GetWindow(get_nursery(), GW_CHILD));

        if (window) {
            ASSERT_HR(webview->put_IsVisible(TRUE));
            SetParent(webview_hwnd, window->hwnd);
            window->resize_everything();
            window->update();
        }
        else {
            SetParent(webview_hwnd, HWND_MESSAGE);
            ASSERT_HR(webview->put_IsVisible(FALSE));
        }

        EventRegistrationToken token;
        ASSERT_HR(webview->add_DocumentTitleChanged(
            Callback<IWebView2DocumentTitleChangedEventHandler>(
                [this](IWebView2WebView* sender, IUnknown* args) -> HRESULT
        {
            wil::unique_cotaskmem_string title;
            webview->get_DocumentTitle(&title);
            tab->set_title(title.get());
            if (window) window->update();
            return S_OK;
        }).Get(), &token));

        ASSERT_HR(webview->add_DocumentStateChanged(
            Callback<IWebView2DocumentStateChangedEventHandler>([this](
                IWebView2WebView* sender,
                IWebView2DocumentStateChangedEventArgs* args) -> HRESULT
        {
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            tab->set_url(source.get());

            BOOL back;
            webview->get_CanGoBack(&back);
            can_go_back = back;

            BOOL forward;
            webview->get_CanGoForward(&forward);
            can_go_forward = forward;

            if (window) window->update();
            return S_OK;
        }).Get(), &token));

        webview->Navigate(tab->url.c_str());

        return S_OK;
    }).Get()));

    all_activities.insert(this);
}

void Activity::set_window (Window* w) {
    if (w == window) return;
    auto old_window = window;
    window = w;
    if (old_window) old_window->set_activity(nullptr);
    if (window) {
        if (webview) {
            ASSERT_HR(webview->put_IsVisible(TRUE));
            SetParent(webview_hwnd, window->hwnd);
        }
        window->set_activity(this);
    }
    else {
        if (webview) {
            SetParent(webview_hwnd, HWND_MESSAGE);
            ASSERT_HR(webview->put_IsVisible(FALSE));
        }
    }
}

void Activity::resize (RECT bounds) {
    if (webview) webview->put_Bounds(bounds);
}

Activity::~Activity () {
    all_activities.erase(this);
    tab->activity = nullptr;
    if (window) window->activity = nullptr;
    webview->Close();
}

