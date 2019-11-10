#include "activities.h"

#include <wrl.h>

#include "assert.h"
#include "json/json.h"
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
            BOOL back;
            page->get_CanGoBack(&back);
            BOOL forward;
            page->get_CanGoForward(&forward);
            if (window) {
                json::Object message {
                    {L"document_state_changed", json::Object{
                        {L"url", url.get()},
                        {L"back", !!back},
                        {L"forward", !!forward}
                    }}
                };
                window->shell->PostWebMessageAsJson(json::stringify(message).c_str());
            }
            return S_OK;
        }).Get(), &token));

        return S_OK;
    }).Get()));
}
