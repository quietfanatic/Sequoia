#include "nursery.h"

#include <wil/com.h>  // For some reason ICoreWebView22.h errors if this isn't included first???
#include <WebView2.h>
#include <WebView2ExperimentalEnvironmentOptions.h>
#include <wrl.h>

#include "activities.h"
#include "model/data.h"
#include "settings.h"
#include "util/assert.h"
#include "util/hash.h"
#include "util/log.h"
#include "util/json.h"
#include "util/text.h"
#include "Window.h"

using namespace Microsoft::WRL;
using namespace std;

wstring edge_udf;
static wil::com_ptr<ICoreWebView2Environment> environment;
HWND nursery_hwnd = nullptr;

wil::com_ptr<ICoreWebView2Controller> next_controller = nullptr;
wil::com_ptr<ICoreWebView2> next_webview = nullptr;
HWND next_hwnd = nullptr;

static auto class_name = L"Sequoia Nursery";

HWND existing_nursery () {
    wstring window_title = L"Sequoia Nursery for " + to_utf16(profile_name);

    return FindWindowExW(HWND_MESSAGE, NULL, class_name, window_title.c_str());
}

static LRESULT CALLBACK WndProcStatic (HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
    case WM_COPYDATA: {
        auto message = json::parse((const char*)((COPYDATASTRUCT*)l)->lpData);
        const string& command = message[0];
        switch (x31_hash(command)) {
        case x31_hash("new_window"): {
            ReplyMessage(0);
            model::Transaction tr;
            model::Page page;
            page.url = string(message[1]);
            page.save();
            model::View view;
            view.root_page = page;
            view.save();
            return 0;
        }
        default: return 1;
        }
    }
    }
    return DefWindowProc(hwnd, message, w, l);
}

void init_nursery () {
    AA(!nursery_hwnd);

    wstring window_title = L"Sequoia Nursery for " + to_utf16(profile_name);

    edge_udf = to_utf16(profile_folder + "/edge-user-data");

    static bool init = []{
        WNDCLASSEXW c {};
        c.cbSize = sizeof(WNDCLASSEX);
        c.lpfnWndProc = WndProcStatic;
        c.hInstance = GetModuleHandle(nullptr);
        c.lpszClassName = class_name;
        AW(RegisterClassExW(&c));
        return true;
    }();
    nursery_hwnd = CreateWindowW(
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
    AW(nursery_hwnd);

     // Check for a race condition where two app processes created a window at the same time.
     // There should only be one nursery window, and it should be the one we just made.
    AA(FindWindowExW(HWND_MESSAGE, NULL, class_name, window_title.c_str()) == nursery_hwnd);
    AA(FindWindowExW(HWND_MESSAGE, nursery_hwnd, class_name, window_title.c_str()) == nullptr);
}

static void create (const function<void(ICoreWebView2Controller*, ICoreWebView2*, HWND)>& then) {
    AH(environment->CreateCoreWebView2Controller(nursery_hwnd,
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [then](HRESULT hr, ICoreWebView2Controller* controller) -> HRESULT
   {
        LOG("Nursery: new webview created");
        AH(hr);
        HWND hwnd = GetWindow(nursery_hwnd, GW_CHILD);
        AW(hwnd);
        SetParent(hwnd, HWND_MESSAGE);

        ICoreWebView2* webview;
        controller->get_CoreWebView2(&webview);
        then(controller, webview, hwnd);
        return S_OK;
    }).Get()));
}

static void queue () {
    create([](ICoreWebView2Controller* controller, ICoreWebView2* webview, HWND hwnd){
        LOG("Nursery: new webview queued");
        next_controller = controller;
        next_webview = webview;
        next_hwnd = hwnd;
    });
}

void new_webview (const function<void(ICoreWebView2Controller*, ICoreWebView2*, HWND)>& then) {
    LOG("Nursery: new webview requested");
    AA(nursery_hwnd);
    if (!environment) {
        LOG("Nursery: creating environment");
        auto options = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
        AH(options->put_AllowSingleSignOnUsingOSPrimaryAccount(TRUE));
        AH(CreateCoreWebView2EnvironmentWithOptions(
            nullptr, edge_udf.c_str(), nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [then](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT
        {
            AH(hr);
            LOG("Nursery: environment created");
            environment = env;
            create(then);
            queue();
            return S_OK;
        }).Get()));
    }
    else if (next_controller) {
        LOG("Nursery: using queued webview");
        auto controller = std::move(next_controller);
        auto webview = std::move(next_webview);
        then(controller.get(), webview.get(), next_hwnd);
        queue();
    }
    else {
        LOG("Nursery: skipping queue");
         // next_webview isn't ready yet.
         // Instead of trying to queue up an arbitrary number of callbacks, just make a
         // new webview ignoring the queue.
        create(then);
    }
}


