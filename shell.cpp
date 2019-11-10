#include "shell.h"

#include <stdexcept>
#include <wrl.h>

#include "activities.h"
#include "assert.h"
#include "json/json.h"
#include "main.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

Shell::Shell (Window* window_) : window(window_) {
    ASSERT_HR(webview_environment->CreateWebView(window->hwnd,
        Callback<IWebView2CreateWebViewCompletedHandler>(
            [this](HRESULT hr, IWebView2WebView* webview_)
    {
        ASSERT_HR(hr);
        webview = webview_;
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
        window->shell_ready();
    }
    else if (command == L"navigate") {
        if (arg.type != json::STRING) throw logic_error("Wrong navigate command arg type");
        if (window->activities.size()) {
            window->activities[0].page->Navigate(arg.string->c_str());
        }
    }
    else {
        throw logic_error("Unknown message name");
    }
}

void Shell::document_state_changed (const wchar_t* url, bool back, bool forward) {
    json::Object message {
        {L"document_state_changed", json::Object{
            {L"url", url},
            {L"back", back},
            {L"forward", forward}
        }}
    };
    webview->PostWebMessageAsJson(json::stringify(message).c_str());
};
