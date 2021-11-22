#include "activity.h"

#include <stdexcept>
#include <wrl.h>

#include "../control/actions.h"
#include "../model/transaction.h"
#include "../util/assert.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "nursery.h"
#include "window.h"

using namespace Microsoft::WRL;
using namespace std;

Activity::Activity (model::PageID p) : page(p) {
    LOG("new Activity", this, page);

    new_webview([this](ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd){
        controller = wvc;
        webview = wv;
        webview_hwnd = hwnd;
        wil::com_ptr<ICoreWebView2Settings> settings;

        if (Window* window = window_for_page(page)) {
            window->resize();
        }

        AH(webview->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
        {
            control::start_loading_page(page);
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_DocumentTitleChanged(
            Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
             // WebView2 tends to have spurious navigations to about:blank, so don't
             // save the title in that case.
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            if (wcscmp(source.get(), L"about:blank") != 0) {
                wil::unique_cotaskmem_string title;
                webview->get_DocumentTitle(&title);
                control::change_page_title(page, from_utf16(title.get()));
            }
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_SourceChanged(
            Callback<ICoreWebView2SourceChangedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2SourceChangedEventArgs* args) -> HRESULT
        {
             // TODO: Make new page instead of changing this one!
             // WebView2 tends to have spurious navigations to about:blank, so don't
             // save the url in that case.
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            if (wcscmp(source.get(), L"") != 0
             && wcscmp(source.get(), L"about:blank") != 0
            ) {
                current_url = from_utf16(source.get());
                if (page->url != current_url) {
                    control::change_page_url(page, current_url);
                }
            }

            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
        {
            control::finish_loading_page(page);
            return S_OK;
        }).Get(), nullptr));

         // This doesn't work for middle-clicking links
         // TODO: How true is that now?  Investigate when this is called.
        AH(webview->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
        {
            wil::unique_cotaskmem_string url;
            args->get_Uri(&url);
            control::open_as_last_child(page, from_utf16(url.get()));
            args->put_Handled(TRUE);
            return S_OK;
        }).Get(), nullptr));

        static std::wstring injection = to_utf16(slurp(exe_relative("res/injection.js")));
        AH(webview->AddScriptToExecuteOnDocumentCreated(injection.c_str(), nullptr));

        AH(webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](
                    ICoreWebView2* sender,
                    ICoreWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw16;
            args->get_WebMessageAsJson(&raw16);
            string raw = from_utf16(raw16.get());
            LOG("Activity::message_from_webview", raw);
            json::Value message = json::parse(raw);
            control::message_from_page(page, message);
            return S_OK;
        }).Get(), nullptr));

        AH(controller->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                [this](
                    ICoreWebView2Controller* sender,
                    ICoreWebView2AcceleratorKeyPressedEventArgs* args
                )
        {
            Window* window = window_for_page(page);
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

        AH(webview->add_ContainsFullScreenElementChanged(
            Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
                [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
            if (page->viewing_view) {
                 // TODO: remove cast
                control::change_view_fullscreen(
                    model::ViewID{page->viewing_view}, is_fullscreen()
                );
            }
            return S_OK;
        }).Get(), nullptr));

        navigate(page->url);
         // TODO: only set this when focusing the page
        control::change_page_visited(page);
    });
}

void Activity::message_to_webview (const json::Value& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_webview", s);
    AH(webview->PostWebMessageAsJson(to_utf16(s).c_str()));
}

void Activity::page_updated () {
    if (page->url != current_url) {
        navigate(page->url);
    }
}

bool Activity::navigate_url (const string& url) {
    LOG("navigate_url", page, url);
    auto hr = webview->Navigate(to_utf16(url).c_str());
    if (SUCCEEDED(hr)) {
        current_url = url;
        return true;
    }
    if (hr != E_INVALIDARG) AH(hr);
    return false;
}

void Activity::navigate_search (const string& search) {
    LOG("navigate_search", page, search);
     // Escape URL characters
    string url = "https://duckduckgo.com/?q=" + escape_url(search);
     // If the search URL is invalid, treat it as a bug
    AA(navigate_url(url));
}

void Activity::navigate (const string& address) {
    if (webview) {
        if (navigate_url(address)) return;
        if (address.find(' ') != string::npos
            || address.find('.') == string::npos
        ) {
            navigate_search(address);
        }
        else {
            string url = "http://" + address;
            if (navigate_url(url)) return;
            navigate_search(address);
        }
    }
    else {
        control::change_page_url(page, current_url = address);
    }
}

bool Activity::is_fullscreen () {
    if (!webview) return false;
    BOOL fs; AH(webview->get_ContainsFullScreenElement(&fs));
    return fs;
}

void Activity::leave_fullscreen () {
    if (!is_fullscreen()) return;
    webview->ExecuteScript(to_utf16("document.exitFullscreen()").c_str(), nullptr);
}

Activity::~Activity () {
    LOG("delete Activity", this);
    if (controller) controller->Close();
}

///// Activity registry

static unordered_map<model::PageID, Activity*> activities_by_page;

Activity* activity_for_page (model::PageID id) {
    auto iter = activities_by_page.find(id);
    if (iter == activities_by_page.end()) return nullptr;
    else return iter->second;
}

struct ActivityRegistry : model::Observer {
    void Observer_after_commit (const model::Update& update) override {
        for (model::PageID page : update.pages) {
            if (page->exists && page->loaded) {
                Activity*& activity = activities_by_page[page];
                if (activity) {
                    activity->page_updated();
                }
                else {
                    activity = new Activity (page);
                }
            }
            else {
                auto iter = activities_by_page.find(page);
                if (iter != activities_by_page.end()) {
                    delete iter->second;
                    activities_by_page.erase(iter);
                }
            }
        }
    }
};
static ActivityRegistry activity_registry;
