#include "nursery.h"

#include <combaseapi.h> // Needs to be included before WebView2.h
#include <WebView2.h>
#include <WebView2ExperimentalEnvironmentOptions.h>
#include <wil/com.h>
#include <wrl.h>

#include "../model/data.h"
#include "../util/error.h"
#include "../util/hash.h"
#include "../util/log.h"
#include "../util/json.h"
#include "../util/text.h"
#include "app.h"
#include "bark.h"
#include "profile.h"

using namespace Microsoft::WRL;
using namespace std;

namespace win32app {

static const wchar_t* class_name = L"Sequoia Nursery";

HWND existing_nursery (const Profile& profile) {
     // TODO: less L strings
    String16 window_title = L"Sequoia Nursery for "sv + to_utf16(profile.name);

    return FindWindowExW(HWND_MESSAGE, NULL, class_name, window_title.c_str());
}

static LRESULT CALLBACK nursery_WndProc (
    HWND hwnd, UINT message, WPARAM w, LPARAM l
) {
    switch (message) {
        case WM_COPYDATA: {
            auto self = (Nursery*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            AA(self);
            auto message = json::parse(
                (const char*)((COPYDATASTRUCT*)l)->lpData
            );
            const string& command = message[0];
            switch (x31_hash(command)) {
                case x31_hash("new_window"): {
                    ReplyMessage(0);
                     // TODO
//                    open_tree_for_urls(write(self->app.model), {message[1]});
                    return 0;
                }
                default: return 1;
            }
        }
        case WM_USER: {
            auto self = (Nursery*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
            AA(self);
             // this ordering prevents reentrancy problems
            auto iter = self->async_queue.begin();
            if (iter != self->async_queue.end()) {
                auto f = std::move(*iter);
                self->async_queue.erase(iter);
                f();
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, message, w, l);
}

Nursery::Nursery (App& a) : app(a) {
    AA(!hwnd);
    AH(OleInitialize(nullptr));

    String16 window_title = L"Sequoia Nursery for "sv
        + to_utf16(app.profile.name);

    edge_udf = to_utf16(app.profile.folder + "/edge-user-data"sv);

    static bool init = []{
        WNDCLASSEXW c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.lpfnWndProc = nursery_WndProc;
        c.hInstance = GetModuleHandle(nullptr);
        c.lpszClassName = class_name;
        AW(RegisterClassExW(&c));
        return true;
    }();
    hwnd = CreateWindowW(
        class_name,
        window_title.c_str(),
        0,
        0, 0,
        0, 0,
        HWND_MESSAGE,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );
    AW(hwnd);
     // Check for a race condition where two app processes created a window at
     // the same time.  There should only be one nursery window, and it should
     // be the one we just made.
    AA(FindWindowExW(
        HWND_MESSAGE, NULL, class_name, window_title.c_str()
    ) == hwnd);
    AA(FindWindowExW(
        HWND_MESSAGE, hwnd, class_name, window_title.c_str()
    ) == nullptr);

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this);
}

Nursery::~Nursery () {
    if (next_controller) {
        next_controller->Close();
    }
}

static void create (
    Nursery& nursery,
    const function<void(ICoreWebView2Controller*, ICoreWebView2*, HWND)>& then
) {
    AH(nursery.environment->CreateCoreWebView2Controller(
        nursery.hwnd,
        Callback<
            ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
        >([&nursery, then](
            HRESULT hr, ICoreWebView2Controller* controller
        ) -> HRESULT {
            if (hr == E_ABORT) {
                 // Nursery was deleted and environment expired.
                return S_OK;
            }

            LOG("Nursery: new webview created"sv);
            AH(hr);
            HWND hwnd = GetWindow(nursery.hwnd, GW_CHILD);
            AW(hwnd);
            SetParent(hwnd, HWND_MESSAGE);

            ICoreWebView2* webview;
            controller->get_CoreWebView2(&webview);
            then(controller, webview, hwnd);
            return S_OK;
        }).Get()
    ));
}

static void queue (
    Nursery& nursery
) {
    create(nursery, [&nursery](
        ICoreWebView2Controller* controller,
        ICoreWebView2* webview,
        HWND hwnd
    ){
        LOG("Nursery: new webview queued"sv);
        nursery.next_controller = controller;
        nursery.next_webview = webview;
        nursery.next_hwnd = hwnd;
    });
}

void Nursery::new_webview (
    const function<void(ICoreWebView2Controller*, ICoreWebView2*, HWND)>& then
) {
    LOG("Nursery: new webview requested"sv);
    AA(hwnd);
    if (!environment) {
        LOG("Nursery: creating environment"sv);
        auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
        AH(options->put_AllowSingleSignOnUsingOSPrimaryAccount(TRUE));
        AH(CreateCoreWebView2EnvironmentWithOptions(
            nullptr, edge_udf.c_str(), nullptr,
            Callback<
                ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
            >([this, then](
                HRESULT hr, ICoreWebView2Environment* env
            ) -> HRESULT {
                AH(hr);
                LOG("Nursery: environment created"sv);
                environment = env;
                create(*this, then);
                queue(*this);
                return S_OK;
            }).Get()
        ));
    }
    else if (next_controller) {
        LOG("Nursery: using queued webview"sv);
         // Async to avoid requiring reentrancy safety
        async([
            this, then,
            controller{std::move(next_controller)},
            webview{std::move(next_webview)},
            hwnd{next_hwnd}
        ] {
            then(controller.get(), webview.get(), hwnd);
            queue(*this);
        });
    }
    else {
        LOG("Nursery: skipping queue"sv);
         // next_webview isn't ready yet.  Instead of trying to queue up an
         // arbitrary number of callbacks, just make a new webview ignoring the
         // queue.
        create(*this, then);
    }
}

void Nursery::async (std::function<void()>&& f) {
    async_queue.emplace_back(std::move(f));
    PostMessage(hwnd, WM_USER, 0, 0);
}

} // namespace win32app
