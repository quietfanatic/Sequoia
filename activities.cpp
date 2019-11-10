#include "activities.h"

#include <wrl.h>

#include "assert.h"
#include "main.h"
#include "Window.h"

using namespace Microsoft::WRL;

Activity::Activity(Window* window_) : window(window_) {
    ASSERT_HR(webview_environment->CreateWebView(window->hwnd,
        Callback<IWebView2CreateWebViewCompletedHandler>(
            [this](HRESULT hr, IWebView2WebView* webview) -> HRESULT
    {
        ASSERT_HR(hr);
        webview->QueryInterface(IID_PPV_ARGS(&page));
        page->Navigate(L"https://duckduckgo.com/");
        if (window) window->resize_everything();

        EventRegistrationToken token;
        ASSERT_HR(page->add_DocumentTitleChanged(
            Callback<IWebView2DocumentTitleChangedEventHandler>(
                [this](
                    IWebView2WebView* sender,
                    IUnknown* args
                ) -> HRESULT
        {
            wil::unique_cotaskmem_string title;
            page->get_DocumentTitle(&title);
            if (window) window->set_title(title.get());
            return S_OK;
        }).Get(), &token));
        ASSERT_HR(page->add_DocumentStateChanged(
            Callback<IWebView2DocumentStateChangedEventHandler>(
                [this](
                    IWebView2WebView* sender,
                    IWebView2DocumentStateChangedEventArgs* args
                ) -> HRESULT
        {
            wil::unique_cotaskmem_string url;
            page->get_Source(&url);
            if (window) window->set_url(url.get());
            return S_OK;
        }).Get(), &token));

        return S_OK;
    }).Get()));
}
