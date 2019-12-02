#include "shell.h"

#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "../_windows.h"
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
    bounds.top += 64;
    bounds.right -= 240;
    return bounds;
}

void Shell::TabObserver_on_commit (const vector<Tab*>& updated_tabs) {
    Array updates;
    updates.reserve(updated_tabs.size());
    for (auto t : updated_tabs) {
        if (t->parent == Tab::DELETING) {
            Tab* s = t->to_focus_on_close();
            if (s) {
                updates.emplace_back(Array{
                    s->id,
                    s->parent,
                    s->next,
                    s->prev,
                    s->child_count,
                    s->title,
                    s->url,
                    true,
                    s->activity && s->activity->can_go_back,
                    s->activity && s->activity->can_go_forward
                });
                window->focus_tab(s);
            }
            else {
                window->focus_tab(nullptr);
            }
            updates.emplace_back(Array{
                t->id,
                Tab::DELETING
            });
        }
        else if (window->tab == t) {
            updates.emplace_back(Array{
                t->id,
                t->parent,
                t->next,
                t->prev,
                t->child_count,
                t->title,
                t->url,
                true,
                t->activity && t->activity->can_go_back,
                t->activity && t->activity->can_go_forward
            });
        }
        else {
            updates.emplace_back(Array{
                t->id,
                t->parent,
                t->next,
                t->prev,
                t->child_count,
                t->title,
                t->url
            });
        }
    }
    message_to_shell(Array{L"update", updates});
};

wstring css_color (uint32 c) {
    char16 buf [8];
    swprintf(buf, 8, L"#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
}

void Shell::message_from_shell (Value&& message) {
    if (message.type != ARRAY) throw logic_error("Unexpected message JSON type");
    if (message.array->size() < 1) throw logic_error("Empty message received from shell");

    if (message[0].type != STRING) throw logic_error("Invalid command JSON type");
    const auto& command = message[0].as<String>();

    switch (x31_hash(command.c_str())) {
    case x31_hash(L"ready"): {
         // Set system colors
        message_to_shell(Array{
            L"colors",
            css_color(GetSysColor(COLOR_ACTIVECAPTION)),
            css_color(GetSysColor(COLOR_CAPTIONTEXT)),
            css_color(GetSysColor(COLOR_3DFACE)),
            css_color(GetSysColor(COLOR_WINDOWTEXT)),
            css_color(GetSysColor(COLOR_3DHIGHLIGHT)),
            css_color(GetSysColor(COLOR_3DSHADOW))
        });
         // Trigger an update to get tab info through TabObserver
        if (window->tab) {
            window->tab->update();
            Tab::commit();
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
    case x31_hash(L"focus"): {
        auto id = int64(message.array->at(1).as<double>());
        if (Tab* tab = Tab::by_id(id)) {
            window->focus_tab(tab);
        }
        else {
            throw std::logic_error("Can't focus non-existent tab?");
        }
        break;
    }
    case x31_hash(L"close"): {
        auto id = int64(message.array->at(1).as<double>());
        if (Tab* tab = Tab::by_id(id)) {
            tab->close();
            Tab::commit();
        }
        else {
            throw std::logic_error("Can't close non-existent tab?");
        }
        break;
    }
    case x31_hash(L"main_menu"): {
        window->show_main_menu();
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

IWebView2WebView4* Shell::active_webview () {
    if (window->activity && window->activity->webview) return window->activity->webview.get();
    else return nullptr;
}
