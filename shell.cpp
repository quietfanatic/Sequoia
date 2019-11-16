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
using namespace json;

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
            char16* raw;
            args->get_WebMessageAsJson(&raw);
            interpret_web_message(parse(raw));
            return S_OK;
        }).Get(), &token);

        webview->Navigate(exe_relative(L"shell.html").c_str());

        window->resize_everything();

        return S_OK;
    }).Get()));
};

void Shell::interpret_web_message (const Value& message) {
    if (message.type != ARRAY) throw logic_error("Unexpected message JSON type");
    if (message.as<Array>().size() < 1) throw logic_error("Empty message received from shell");
    if (message[0].type != STRING) throw logic_error("Invalid command JSON type");
    auto& command = message[0].as<String>();

    auto arg = [&](int i){
        if (message.as<Array>().size() < i + 2) {
            throw logic_error("Missing required message argument from shell");
        }
        return message[i+1];
    };

    if (command == L"ready") {
        window->update();
    }
    else if (command == L"navigate") {
        auto& url = arg(0).as<String>();
        if (window->activity && window->activity->webview) {
            window->activity->webview->Navigate(url.c_str());
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
    Array message {L"update", url, back, forward};
    webview->PostWebMessageAsJson(stringify(message).c_str());
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
