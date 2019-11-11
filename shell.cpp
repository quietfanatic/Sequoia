#include "shell.h"

#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "activities.h"
#include "assert.h"
#include "json/json.h"
#include "main.h"
#include "tabs.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

Shell::Shell (Window* owner) : window(owner) {
    ASSERT_HR(webview_environment->CreateWebView(window->hwnd,
        Callback<IWebView2CreateWebViewCompletedHandler>(
            [this](HRESULT hr, IWebView2WebView* wv)
    {
        ASSERT_HR(hr);
        ASSERT_HR(wv->QueryInterface(IID_PPV_ARGS(&webview)));
        ASSERT(webview_hwnd = GetWindow(window->hwnd, GW_CHILD));
        EventRegistrationToken token;
        webview->add_WebMessageReceived(
            Callback<IWebView2WebMessageReceivedEventHandler>(
                [this](
                    IWebView2WebView* sender,
                    IWebView2WebMessageReceivedEventArgs* args
                )
        {
            wchar_t* raw;
            args->get_WebMessageAsJson(&raw);
            interpret_web_message(json::parse(raw));
            return S_OK;
        }).Get(), &token);

        webview->Navigate(exe_relative(L"shell.html").c_str());

        window->resize_everything();

        return S_OK;
    }).Get()));
};

void Shell::interpret_web_message (const json::Value& message) {
    if (message.type != json::OBJECT) throw logic_error("Unexpected message JSON type");
    if (message.object->size() != 1) throw logic_error("Wrong size of message object");
    auto command = (*message.object)[0].first;
    auto arg = (*message.object)[0].second;

    if (command == L"ready") {
        window->update();
    }
    else if (command == L"navigate") {
        if (arg.type != json::STRING) throw logic_error("Wrong navigate command arg type");
        if (window->activity && window->activity->webview) {
            window->activity->webview->Navigate(arg.string->c_str());
        }
    }
    else {
        throw logic_error("Unknown message name");
    }
}

void Shell::update () {
    if (!webview) return;
    auto tab = window->tab;
    auto url = tab ? tab->url.c_str() : L"";
    auto back = tab && tab->activity && tab->activity->can_go_back;
    auto forward = tab && tab->activity && tab->activity->can_go_forward;
    json::Object message {
        {L"update", json::Object{
            {L"url", url},
            {L"back", back},
            {L"forward", forward}
        }}
    };
    webview->PostWebMessageAsJson(json::stringify(message).c_str());
};

RECT Shell::resize (RECT bounds) {
    if (webview) {
        webview->put_Bounds(bounds);
         // Put shell behind the page
        SetWindowPos(
            webview_hwnd, HWND_BOTTOM,
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
        );
    }
    bounds.top += 68;
    bounds.right -= 244;
    return bounds;
}
