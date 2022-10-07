#include "activities.h"

#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "../model/data.h"
#include "../util/error.h"
#include "../util/files.h"
#include "../util/hash.h"
#include "../util/json.h"
#include "../util/log.h"
#include "../util/text.h"
#include "app.h"
#include "bark.h"
#include "nursery.h"

using namespace Microsoft::WRL;
using namespace std;

namespace win32app {

static map<int64, Activity*> activities_by_tab;

Activity::Activity (int64 t) : tab(t) {
    LOG("new Activity", this);
    AA(!activities_by_tab.contains(t));
    activities_by_tab.emplace(t, this);

    App::get().nursery.new_webview([this](ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd){
        controller = wvc;
        webview = wv;
        webview_hwnd = hwnd;
        wil::com_ptr<ICoreWebView2Settings> settings;

        claimed_by_bark(bark);
        if (bark) {
            bark->resize();
            if (get_tab_data(tab)->url != "about:blank") {
                AH(controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC));
            }
        }

        AH(webview->add_NavigationStarting(
            Callback<ICoreWebView2NavigationStartingEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
        {
            currently_loading = true;
            tab_updated(tab);
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
                set_tab_title(tab, from_utf16(title.get()));
            }
            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_HistoryChanged(
            Callback<ICoreWebView2HistoryChangedEventHandler>(
                [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
            BOOL back;
            webview->get_CanGoBack(&back);
            can_go_back = back;

            BOOL forward;
            webview->get_CanGoForward(&forward);
            can_go_forward = forward;

            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_SourceChanged(
            Callback<ICoreWebView2SourceChangedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2SourceChangedEventArgs* args) -> HRESULT
        {
             // WebView2 tends to have spurious navigations to about:blank, so don't
             // save the url in that case.
            wil::unique_cotaskmem_string source;
            webview->get_Source(&source);
            if (wcscmp(source.get(), L"") != 0
             && wcscmp(source.get(), L"about:blank") != 0
            ) {
                navigated_url = "";
                set_tab_url(tab, from_utf16(source.get()));
            }
            else {
                set_tab_url(tab, navigated_url);
            }

            return S_OK;
        }).Get(), nullptr));

        AH(webview->add_NavigationCompleted(
            Callback<ICoreWebView2NavigationCompletedEventHandler>([this](
                ICoreWebView2* sender,
                ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
        {
            currently_loading = false;
            tab_updated(tab);
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
            create_tab(tab, TabRelation::LAST_CHILD, from_utf16(url.get()));
            args->put_Handled(TRUE);
            return S_OK;
        }).Get(), nullptr));

        static std::wstring injection = to_utf16(slurp(exe_relative("res/win32app/injection.js")));

        AH(webview->AddScriptToExecuteOnDocumentCreated(injection.c_str(), nullptr));

        AH(webview->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](
                    ICoreWebView2* sender,
                    ICoreWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw;
            args->get_WebMessageAsJson(&raw);
            message_from_webview(json::parse(from_utf16(raw.get())));
            return S_OK;
        }).Get(), nullptr));

        AH(controller->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                [this](
                    ICoreWebView2Controller* sender,
                    ICoreWebView2AcceleratorKeyPressedEventArgs* args
                )
        {
            if (!bark) return S_OK;
            return bark->on_AcceleratorKeyPressed(sender, args);
        }).Get(), nullptr));

        AH(webview->add_ContainsFullScreenElementChanged(
            Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
                [this](ICoreWebView2* sender, IUnknown* args) -> HRESULT
        {
            is_fullscreen() ? bark->enter_fullscreen() : bark->leave_fullscreen();
            return S_OK;
        }).Get(), nullptr));

        navigate_url_or_search(get_tab_data(tab)->url);
        set_tab_visited(tab);
    });

     // Delete old activities
     // TODO: configurable values
    while (activities_by_tab.size() > 80) {
        set<int64> keep_loaded;
         // Don't unload self!
        keep_loaded.emplace(tab);
         // Don't unload tabs focused by any windows
        for (auto w : get_all_unclosed_windows()) {
            keep_loaded.emplace(get_window_data(w)->focused_tab);
        }
         // Keep last n loaded tabs regardless
        for (auto t : get_last_visited_tabs(20)) {
            keep_loaded.emplace(t);
        }
         // Find last unstarred tab or last starred tab
        int64 victim_id = 0;
        TabData* victim_dat = nullptr;
        for (auto p : activities_by_tab) {
            if (keep_loaded.contains(p.first)) continue;
            auto dat = get_tab_data(p.first);
             // Not yet visited?  Not quite sure why this would happen.
            if (dat->visited_at == 0) continue;
            if (!victim_id
                || !dat->starred_at && victim_dat->starred_at
                || dat->visited_at < victim_dat->visited_at
            ) {
                victim_id = p.first;
                victim_dat = dat;
            }
        }
        delete activity_for_tab(victim_id);
    }
}

void Activity::message_from_webview(json::Value&& message) {
    Str command = message[0];

    switch (x31_hash(command)) {
    case x31_hash("favicon"): {
        Str favicon = message[1];
        set_tab_favicon(tab, favicon);
        break;
    }
    case x31_hash("click_link"): {
        Str url = message[1];
        Str title = message[2];
        int button = message[3];
        bool double_click = message[4];
        bool shift = message[5];
        bool alt = message[6];
        bool ctrl = message[7];
        if (button == 1) {
            if (double_click) {
                if (last_created_new_child && bark) {
                    bark->focus_tab(last_created_new_child);
                }
            }
            else if (alt && shift) {
                last_created_new_child = create_tab(tab, TabRelation::BEFORE, url, title);
            }
            else if (alt) {
                last_created_new_child = create_tab(tab, TabRelation::AFTER, url, title);
            }
            else if (shift) {
                last_created_new_child = create_tab(tab, TabRelation::FIRST_CHILD, url, title);
            }
            else {
                last_created_new_child = create_tab(tab, TabRelation::LAST_CHILD, url, title);
            }
        }
        break;
    }
    case x31_hash("new_children"): {
        last_created_new_child = 0;
        const json::Array& children = message[1];
        Transaction tr;
        for (auto& child : children) {
            Str url = child[0];
            Str title = child[1];
            create_tab(tab, TabRelation::LAST_CHILD, url, title);
        }
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }
}

void Activity::message_to_webview (json::Value&& message) {
    if (!webview) return;
    auto s = json::stringify(message);
    LOG("message_to_webview", s);
    AH(webview->PostWebMessageAsJson(to_utf16(s).c_str()));
}

void Activity::claimed_by_bark (Bark* w) {
    bark = w;
    if (controller) {
        if (bark) {
            AH(controller->put_IsVisible(TRUE));
            SetParent(webview_hwnd, bark->os_window.hwnd);
        }
        else {
            AH(controller->put_IsVisible(FALSE));
            auto wv8 = webview.try_query<ICoreWebView2_8>();
            AA(!!wv8);
            BOOL playing_audio;
            AH(wv8->get_IsDocumentPlayingAudio(&playing_audio));
            if (!playing_audio) {
                AH(wv8->TrySuspend(Callback<ICoreWebView2TrySuspendCompletedHandler>(
                    [this](HRESULT hr, BOOL success){
                        AH(hr);
                        LOG("Suspend tab", tab, success);
                        return S_OK;
                    }
                ).Get()));
            }
            SetParent(webview_hwnd, HWND_MESSAGE);
        }
    }
}

void Activity::resize (RECT bounds) {
    if (controller) controller->put_Bounds(bounds);
}

bool Activity::navigate_url (Str address) {
    auto hr = webview->Navigate(to_utf16(address).c_str());
    if (SUCCEEDED(hr)) {
        navigated_url = address;
        return true;
    }
    if (hr != E_INVALIDARG) AH(hr);
    return false;
}
void Activity::navigate_search (Str search) {
     // Escape URL characters
    String url = "https://duckduckgo.com/?q=" + escape_url(search);
     // If the search URL is invalid, treat it as a bug
    AA(navigate_url(url));
}

void Activity::navigate_url_or_search (Str address) {
    LOG("navigate_url_or_search", address);
    if (webview) {
        if (navigate_url(address)) return;
        if (address.find(' ') != String::npos
            || address.find('.') == String::npos
        ) {
            navigate_search(address);
        }
        else {
            String url = "http://" + address;
            if (navigate_url(url)) return;
            navigate_search(address);
        }
    }
    else {
        set_tab_url(tab, address);
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
    activities_by_tab.erase(tab);
    if (bark) bark->activity = nullptr;
    if (controller) controller->Close();
    tab_updated(tab);
}

Activity* activity_for_tab (int64 id) {
    auto iter = activities_by_tab.find(id);
    if (iter == activities_by_tab.end()) return nullptr;
    else return iter->second;
}

Activity* ensure_activity_for_tab (int64 id) {
    auto iter = activities_by_tab.find(id);
    if (iter == activities_by_tab.end()) {
        iter = activities_by_tab.emplace(id, new Activity(id)).first;
    }
    return iter->second;
}

} // namespace win32app
