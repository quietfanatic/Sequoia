#include "shell.h"

#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "../activities.h"
#include "../assert.h"
#include "../hash.h"
#include "../json/json.h"
#include "../main.h"
#include "../tabs.h"
#include "../Window.h"

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
            message_from_shell(parse(raw));
            return S_OK;
        }).Get(), &token);

        webview->Navigate(exe_relative(L"shell/shell.html").c_str());

        window->resize_everything();

        return S_OK;
    }).Get()));
};

void Shell::message_from_shell (Value&& message) {
    if (message.type != ARRAY) throw logic_error("Unexpected message JSON type");
    if (message.array->size() < 1) throw logic_error("Empty message received from shell");

    if (message[0].type != STRING) throw logic_error("Invalid command JSON type");
    const auto& command = message[0].as<String>();

    switch (x31_hash(command.c_str())) {
    case x31_hash(L"ready"): {
        if (window->tab) {
            window->update_tab(window->tab);
        }
        break;
    }
    case x31_hash(L"navigate"): {
        const auto& url = message.array->at(1).as<String>();
        if (auto wv = active_webview()) wv->Navigate(url.c_str());
        break;
    }
    case x31_hash(L"back"): {
        if (auto wv = active_webview()) wv->GoBack();
        break;
    }
    case x31_hash(L"forward"): {
        if (auto wv = active_webview()) wv->GoForward();
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }
}

void Shell::message_to_shell (Value&& message) {
    if (!webview) return;
    webview->PostWebMessageAsJson(stringify(message).c_str());
}

void Shell::update_tabs (Tab** tabs, size_t length) {
    Array updates;
    updates.reserve(length);
    for (size_t i = 0; i < length; i++) {
        Tab* tab = tabs[i];
        if (window->tab == tab) {
            updates.emplace_back(Array{
                tab->id,
                tab->parent,
                tab->next,
                tab->prev,
                tab->child_count,
                tab->title,
                tab->url,
                true,
                tab->activity && tab->activity->can_go_back,
                tab->activity && tab->activity->can_go_forward
            });
        }
        else {
            updates.emplace_back(Array{
                tab->id,
                tab->parent,
                tab->next,
                tab->prev,
                tab->child_count,
                tab->title,
                tab->url
            });
        }
    }
    message_to_shell(Array{L"update", updates});
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

IWebView2WebView4* Shell::active_webview () {
    if (window->activity && window->activity->webview) return window->activity->webview.get();
    else return nullptr;
}
