#include "activity.h"

#include <stdexcept>
#include <webview2.h>
#include <wrl.h>

#include "../model/edge.h"
#include "../model/node.h"
#include "../model/view.h"
#include "../model/write.h"
#include "../util/error.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "app.h"
#include "nursery.h"
#include "window.h"

using namespace Microsoft::WRL;
using namespace std;

namespace win32app {

Activity::Activity (App& a, model::ActivityID i) : app(a), id(i) {
    LOG("new Activity"sv, this, id);
    AA(id);

    // Theoretically this activity could be destroyed before the webview
    // creation finished, so use a weak pointer to keep track of it.
    // The event handlers are fine because the activity closes the webview
    // in its destructor, so no events will fire after that.
    WeakPtr<Activity> weak_self = this;
    app.nursery.new_webview([weak_self](
        ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd
    ) {
        Activity* self = weak_self;
        if (!self) {
            wvc->Close();
            return;
        }
        self->controller = wvc;
        self->webview = wv;
        self->webview_hwnd = hwnd;

         // TODO: add NavigationStarting to start_loading
        AH(self->webview->add_DocumentTitleChanged(
            Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                [self](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
             // WebView2 tends to have spurious navigations to about:blank,
             // so don't save the title in that case.
            wil::unique_cotaskmem_string source;
            self->webview->get_Source(&source);
            if (source.get() != L"about:blank"sv) {
                wil::unique_cotaskmem_string title;
                self->webview->get_DocumentTitle(&title);
                title_changed(write(self->app.model),
                    self->id, from_utf16(title.get())
                );
            }
            return S_OK;
        }).Get(), nullptr));

        AH(self->webview->add_SourceChanged(
            Callback<ICoreWebView2SourceChangedEventHandler>([self](
                ICoreWebView2* sender,
                ICoreWebView2SourceChangedEventArgs* args) -> HRESULT
        {
             // WebView2 tends to have spurious navigations to about:blank,
             // so don't save the url in that case.
            wil::unique_cotaskmem_string source;
            self->webview->get_Source(&source);
            if (source.get() != L""sv && source.get() != L"about:blank"sv) {
                self->current_url = from_utf16(source.get());
                url_changed(write(self->app.model),
                    self->id, self->current_url
                );
            }

            return S_OK;
        }).Get(), nullptr));

        AH(self->webview->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>([self](
                ICoreWebView2* sender,
                ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
        {
            self->current_loading_at = 0;
            finished_loading(write(self->app.model), self->id);
            return S_OK;
        }).Get(), nullptr));

         // This doesn't work for middle-clicking edges
         // TODO: How true is that now?  Investigate when this is called.
        AH(self->webview->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>([self](
                ICoreWebView2* sender,
                ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
        {
            wil::unique_cotaskmem_string url;
            args->get_Uri(&url);

            open_last_child(write(self->app.model),
                self->id, from_utf16(url.get())
            );

            args->put_Handled(TRUE);
            return S_OK;
        }).Get(), nullptr));

        static std::wstring injection = to_utf16(
            slurp(exe_relative("res/win32app/activity.js"sv))
        );
        AH(self->webview->AddScriptToExecuteOnDocumentCreated(
            injection.c_str(), nullptr
        ));

        AH(self->webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [self](
                    ICoreWebView2* sender,
                    ICoreWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw16;
            args->get_WebMessageAsJson(&raw16);
            string raw = from_utf16(raw16.get());
            LOG("Activity::message_from_webview"sv, raw);
            json::Value message = json::parse(raw);
            self->message_from_webview(message);
            return S_OK;
        }).Get(), nullptr));

        AH(self->controller->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                [self](
                    ICoreWebView2Controller* sender,
                    ICoreWebView2AcceleratorKeyPressedEventArgs* args
                )
        {
            // TODO: keep track of viewing view
            Window* window = nullptr;
            if (!window) return S_OK;
            COREWEBVIEW2_KEY_EVENT_KIND kind;
            AH(args->get_KeyEventKind(&kind));
            switch (kind) {
                case COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN: break;
                case COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN: break;
                default: return S_OK;
            }
            UINT key; AH(args->get_VirtualKey(&key));
            auto handle = window->get_key_handler(
                key,
                GetKeyState(VK_SHIFT) < 0,
                GetKeyState(VK_CONTROL) < 0,
                GetKeyState(VK_MENU) < 0
            );
            if (handle) {
                AH(args->put_Handled(TRUE));
                INT l; AH(args->get_KeyEventLParam(&l));
                bool repeated = l & (1 << 30);
                if (!repeated) handle();
            }
            return S_OK;
        }).Get(), nullptr));

        AH(self->webview->add_ContainsFullScreenElementChanged(
            Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
                [self](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
            // TODO: Keep track of viewing view and set fullscreen
            return S_OK;
        }).Get(), nullptr));

        auto data = self->app.model/self->id;
        if (data->view) {
            self->app.window_for_view(data->view)->reflow();
        }
        self->update();
        if (self->waiting_for_ready) {
            self->waiting_for_ready = false;
            self->app.quit();
        }
    });
}

void Activity::message_to_webview (const json::Value& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_webview"sv, s);
    AH(webview->PostWebMessageAsJson(to_utf16(s).c_str()));
}

void Activity::message_from_webview (const json::Value& message) {
    Str command = message[0];

    switch (x31_hash(command)) {
        case x31_hash("favicon"): {
            favicon_url_changed(write(app.model), id, message[1]);
            break;
        }
        case x31_hash("click_edge"): {
             // TODO: separate clicking from navigating (gather intents from
             // clicks, then let the website open links, then map those links
             // to intents).
            std::string url = message[1];
            std::string title = message[2];
            int button = message[3];
            bool double_click = message[4];
            bool shift = message[5];
            bool alt = message[6];
            bool ctrl = message[7];
            if (button == 1) {
                if (double_click) {
                     // TODO: focus last created tab
                }
                else {
                    auto w = write(app.model);
                    if (alt) {
                        if (shift) {
                            open_prev_sibling(w, id, url, title);
                        }
                        else {
                            open_next_sibling(w, id, url, title);
                        }
                    }
                    else {
                        if (shift) {
                            open_first_child(w, id, url, title);
                        }
                        else {
                            open_last_child(w, id, url, title);
                        }
                    }
                }
            }
            break;
        }
        case x31_hash("new_children"): {
         // TODO
    //        last_created_new_child = 0;
    //        const json::Array& children = message[1];
    //        Transaction tr;
    //        for (auto& child : children) {
    //            Str url = child[0];
    //            Str title = child[1];
    //            create_tab(tab, TabRelation::LAST_CHILD, url, title);
    //        }
            break;
        }
        default: {
            ERR("Unknown message name"sv);
        }
    }
}

static bool navigate_url (Activity& self, Str url) {
    LOG("navigate_url"sv, self.id, url);
    auto hr = self.webview->Navigate(to_utf16(url).c_str());
    if (SUCCEEDED(hr)) {
        self.current_url = url;
        return true;
    }
    if (hr != E_INVALIDARG) AH(hr);
    return false;
}

static void navigate_search (Activity& self, Str search) {
    LOG("navigate_search"sv, self.id, search);
     // Escape URL characters
    String url = "https://duckduckgo.com/?q="sv + escape_url(search);
     // If the search URL is invalid, treat it as a bug
    AA(navigate_url(self, url));
}

static void navigate (Activity& self, Str address) {
    AA(self.webview);
    if (navigate_url(self, address)) return;
    if (address.find(' ') != string::npos
        || address.find('.') == string::npos
    ) {
        navigate_search(self, address);
    }
    else {
        String url = "http://"sv + address;
        if (navigate_url(self, url)) return;
        navigate_search(self, address);
    }
}

void Activity::update () {
    if (!webview) {
         // We'll come back here when the webview is ready.
        return;
    }
    auto data = app.model/id;
    AA(data);
    if (data->loading_at) {
        AA(data->loading_at >= current_loading_at);
        if (data->loading_at > current_loading_at) {
            current_loading_at = data->loading_at;
            if (!data->loading_address.empty()) {
                AA(!data->reloading);
                navigate(*this, data->loading_address);
            }
            else if (data->reloading) {
                webview->Reload();
            }
            else {
                 // This will happen when loading an existing node.
                AA(data->node);
                auto node_data = app.model/data->node;
                navigate_url(*this, node_data->url);
            }
        }
    }
    else if (current_loading_at) {
        webview->Stop();
        current_loading_at = 0;
    }
}

bool Activity::is_fullscreen () {
    if (!webview) return false;
    BOOL fs; AH(webview->get_ContainsFullScreenElement(&fs));
    return fs;
}

void Activity::leave_fullscreen () {
    if (!is_fullscreen()) return;
    webview->ExecuteScript(
        to_utf16("document.exitFullscreen()"sv).c_str(),
        nullptr
    );
}

void Activity::wait_for_ready () {
    if (webview) return;
    waiting_for_ready = true;
    app.run();
}

Activity::~Activity () {
    LOG("delete Activity"sv, this);
    if (controller) controller->Close();
}

} // namespace win32app
