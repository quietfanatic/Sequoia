#include "Window.h"

#include <unordered_set>
#include <stdexcept>
#include <WebView2.h>
#include <wrl.h>

#include "activities.h"
#include "data.h"
#include "main.h"
#include "nursery.h"
#include "settings.h"
#include "util/assert.h"
#include "util/files.h"
#include "util/hash.h"
#include "util/json.h"
#include "util/log.h"
#include "util/text.h"
#include "util/time.h"

using namespace Microsoft::WRL;
using namespace std;

///// WINDOW STUFF

static std::unordered_map<ViewID, Window*> open_windows;

static void gen_visible_tabs (Window& w) {
    w.visible_tabs.clear();
    vector<Tab> tabs_to_scan {Tab{LinkID{}, w.view.root_page, LinkID{}}};
    for (size_t i = 0; i < tabs_to_scan.size(); i++) {
        Tab& tab = tabs_to_scan[i];
        w.visible_tabs.emplace(tab.id, tab);
        if (w.view.expanded_tabs.count(tab.id)) {
             // TODO: get_links_to_page also
            for (LinkID l : get_links_from_page(tab.page)) {
                if (!w.visible_tabs.count(l)) {
                    tabs_to_scan.push_back(Tab{l, l.load().to_page, tab.id});
                }
            }
        }
    }
}

Window::Window (ViewID v) :
    view(v.load()), os_window(this)
{
    open_windows.emplace(view.id, this);
     // TODO: Fix possible use-after-free of this
    new_webview([this](ICoreWebView2Controller* wvc, ICoreWebView2* wv, HWND hwnd){
        shell_controller = wvc;
        shell = wv;
        shell_hwnd = hwnd;
        SetParent(shell_hwnd, os_window.hwnd);
        shell_controller->put_IsVisible(TRUE);

        shell->add_WebMessageReceived(
            Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                [this](
                    ICoreWebView2* sender,
                    ICoreWebView2WebMessageReceivedEventArgs* args
                )
        {
            wil::unique_cotaskmem_string raw16;
            args->get_WebMessageAsJson(&raw16);
            string raw = from_utf16(raw16.get());
            LOG("message_from_shell", raw);
            message_from_shell(json::parse(raw));
            return S_OK;
        }).Get(), nullptr);

        shell_controller->add_AcceleratorKeyPressed(
            Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                this, &Window::on_AcceleratorKeyPressed
            ).Get(), nullptr
        );

        shell->Navigate(to_utf16(exe_relative("res/shell.html")).c_str());

        resize();
    });
    gen_visible_tabs(*this);
};

Window::~Window () {
    if (activity) activity->claimed_by_window(nullptr);
    open_windows.erase(view.id);
    if (open_windows.empty()) quit();
}

void Window::claim_activity (Activity* a) {
    if (a == activity) return;

    if (activity) activity->claimed_by_window(nullptr);
    activity = a;
    if (activity) activity->claimed_by_window(this);

    resize();
}

void Window::on_hidden () {
    LOG("Window::on_hidden");
    if (activity && activity->controller) {
        activity->controller->put_IsVisible(FALSE);
    }
     // TODO: change to FALSE
    if (shell_controller) shell_controller->put_IsVisible(TRUE);
}

void Window::resize () {
    if (activity && activity->controller) {
        activity->controller->put_IsVisible(TRUE);
    }
    if (shell_controller) shell_controller->put_IsVisible(TRUE);

    RECT bounds;
    GetClientRect(os_window.hwnd, &bounds);
    if (shell_controller) {
        shell_controller->put_Bounds(bounds);
         // Put shell behind the page
        SetWindowPos(
            shell_hwnd, HWND_BOTTOM,
            0, 0, 0, 0,
            SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE
        );
    }
    auto dpi = GetDpiForWindow(os_window.hwnd);
    AW(dpi);
    double scale = dpi / 96.0;
    if (!fullscreen) {
        bounds.top += uint(toolbar_height * scale);
        double side_width = sidebar_width > main_menu_width ? sidebar_width : main_menu_width;
        bounds.right -= uint(side_width * scale);
    }
    if (activity) {
        activity->resize(bounds);
    }
}

void Window::enter_fullscreen () {
    if (fullscreen) return;
    if (!activity) return;
    fullscreen = true;
    os_window.enter_fullscreen();
    shell_controller->put_IsVisible(FALSE);
}

void Window::leave_fullscreen () {
    if (!fullscreen) return;
    fullscreen = false;
    shell_controller->put_IsVisible(TRUE);
    os_window.leave_fullscreen();
    if (activity) activity->leave_fullscreen();
}

void Window::close () {
    view.closed_at = now();
    view.save();
}

///// CONTROLLER STUFF

HRESULT Window::on_AcceleratorKeyPressed (
    ICoreWebView2Controller* sender,
    ICoreWebView2AcceleratorKeyPressedEventArgs* args
) {
    COREWEBVIEW2_KEY_EVENT_KIND kind;
    AH(args->get_KeyEventKind(&kind));
    switch (kind) {
    case COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN: break;
    case COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN: break;
    default: return S_OK;
    }
    UINT key; AH(args->get_VirtualKey(&key));
    INT l; AH(args->get_KeyEventLParam(&l));
    auto handle = get_key_handler(
        key,
        GetKeyState(VK_SHIFT) < 0,
        GetKeyState(VK_CONTROL) < 0,
        GetKeyState(VK_MENU) < 0
    );
    if (handle) {
        AH(args->put_Handled(TRUE));
        bool repeated = l & (1 << 30);
        if (!repeated) handle();
    }
    return S_OK;
}

std::function<void()> Window::get_key_handler (uint key, bool shift, bool ctrl, bool alt) {
    switch (key) {
    case VK_F11:
        if (!shift && !ctrl && !alt) return [this]{
            fullscreen ? leave_fullscreen() : enter_fullscreen();
        };
        break;
    case 'L':
        if (!shift && ctrl && !alt) return [this]{
            if (!shell) return;
            message_to_shell(json::array("select_location"));
            shell_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        };
        break;
    case 'N':
        if (ctrl && !alt) {
            if (shift) return [this]{
//                Transaction tr;
//                if (int64 w = get_last_closed_window()) {
//                    unclose_window(w);
//                    unclose_tab(get_window_data(w)->focused_tab);
//                }
            };
            else return [this]{
//                Transaction tr;
//                int64 new_tab = create_tab(0, TabRelation::LAST_CHILD, "about:blank");
//                int64 new_window = create_window(new_tab, new_tab);
            };
        }
        break;
    case 'T':
        if (ctrl && !alt) {
            if (shift) return [this]{
//                Transaction tr;
//                if (int64 tab = get_last_closed_tab()) {
//                    unclose_tab(tab);
//                    set_window_focused_tab(id, tab);
//                    claim_activity(ensure_activity_for_tab(tab));
//                }
            };
            else return [this]{
//                Transaction tr;
//                int64 new_tab = create_tab(
//                    get_window_data(id)->focused_tab,
//                    TabRelation::LAST_CHILD,
//                    "about:blank"
//                );
//                set_window_focused_tab(id, new_tab);
//                claim_activity(ensure_activity_for_tab(new_tab));
            };
        }
        break;
    case 'U':
        if (!shift && ctrl && !alt) return [this]{
            if (activity) {
                delete activity;
            }
        };
        break;
    case 'W':
        if (!shift && ctrl && !alt) return [this]{
//            close_tab_with_heritage(get_window_data(id)->focused_tab);
        };
        break;
    case VK_TAB:
        if (ctrl && !alt) {
            if (shift) return [this]{
//                int64 prev = get_prev_unclosed_tab(get_window_data(id)->focused_tab);
//                if (prev) focus_tab(prev);
            };
            else return [this]{
//                int64 next = get_next_unclosed_tab(get_window_data(id)->focused_tab);
//                if (next) focus_tab(next);
            };
        }
        break;
    case VK_ESCAPE:
        if (!shift && !ctrl && !alt) {
            if (fullscreen) return [this]{
                leave_fullscreen();
            };
        }
        break;
    case VK_UP:
        if (!shift && ctrl && !alt) {
            return [this]{
//                int64 prev = get_prev_unclosed_tab(get_window_data(id)->focused_tab);
//                if (prev) focus_tab(prev);
            };
        }
        break;
    case VK_DOWN:
        if (!shift && ctrl && !alt) {
            return [this]{
//                int64 next = get_next_unclosed_tab(get_window_data(id)->focused_tab);
//                if (next) focus_tab(next);
            };
        }
        break;
    }
    return nullptr;
}

void Window::message_from_shell (json::Value&& message) {
    try {

    const string& command = message[0];

    switch (x31_hash(command.c_str())) {
    case x31_hash("ready"): {
        message_to_shell(json::array(
            "settings",
            json::Object{
                std::pair{"theme", settings::theme}
            }
        ));
        send_view();
        ready = true;
        break;
    }
    case x31_hash("resize"): {
        sidebar_width = uint(message[1]);
        toolbar_height = uint(message[2]);
        resize();
        break;
    }
    case x31_hash("navigate"): {
        Transaction tr;
        claim_activity(ensure_activity_for_page(view.focused_page()));
        activity->navigate_url_or_search(message[1]);
        break;
    }
     // Toolbar buttons
    case x31_hash("back"): {
        if (activity && activity->webview) {
            activity->webview->GoBack();
        }
        break;
    }
    case x31_hash("forward"): {
        if (activity && activity->webview) {
            activity->webview->GoForward();
        }
        break;
    }
    case x31_hash("reload"): {
        if (activity && activity->webview) {
            activity->webview->Reload();
        }
        break;
    }
    case x31_hash("stop"): {
        if (activity && activity->webview) {
            activity->webview->Stop();
        }
        break;
    }
    case x31_hash("investigate_error"): {
        shell->OpenDevToolsWindow();
        break;
    }
    case x31_hash("show_menu"): {
        main_menu_width = uint(message[1]);
        resize();
        break;
    }
    case x31_hash("hide_menu"): {
        main_menu_width = 0;
        resize();
        break;
    }
     // Tab actions
    case x31_hash("focus_tab"): {
        Transaction tr;
        view.focused_tab = LinkID{message[1]};
        view.save();
        claim_activity(ensure_activity_for_page(view.focused_page()));
        break;
    }
    case x31_hash("new_child"): {
        LinkID parent_tab {message[1]};
        PageID parent_page = parent_tab ? parent_tab.load().to_page : view.root_page;
        Transaction tr;
        PageData child;
        child.url = "about:blank";
        child.method = Method::Get;
        child.save();
        LinkData link;
        link.opener_page = parent_page;
        link.from_page = parent_page;
        link.to_page = child;
        link.move_last_child(parent_page);
        link.save();
        view.focused_tab = link;
        view.expanded_tabs.insert(parent_tab);
        view.save();
        claim_activity(ensure_activity_for_page(child));
        break;
    }
    case x31_hash("trash_link"): {
        LinkData link = LinkID{message[1]}.load();
        link.trashed_at = now();
        link.save();
         // TODO: delete activities that aren't visible in any views
        break;
    }
    case x31_hash("delete_link"): {
        LinkData link = LinkID{message[1]}.load();
        if (link.trashed_at) {
            link.exists = false;
            link.save();
        }
        break;
    }
    case x31_hash("move_link_before"): {
        LinkData link = LinkID{message[1]}.load();
        LinkID ref {message[2]};
        link.move_before(ref);
        link.save();
        break;
    }
    case x31_hash("move_link_after"): {
        LinkData link = LinkID{message[1]}.load();
        LinkID ref {message[2]};
        link.move_after(ref);
        link.save();
        break;
    }
    case x31_hash("move_link_first_child"): {
        LinkData link = LinkID{message[1]}.load();
        PageID ref {message[2]};
        link.move_first_child(ref);
        link.save();
        break;
    }
    case x31_hash("move_link_last_child"): {
        LinkData link = LinkID{message[1]}.load();
        PageID ref {message[2]};
        link.move_last_child(ref);
        link.save();
        break;
    }
    case x31_hash("expand_link"): {
        view.expanded_tabs.insert(LinkID{message[1]});
        view.save();
        break;
    }
    case x31_hash("contract_link"): {
        view.expanded_tabs.erase(LinkID{message[1]});
        view.save();
        break;
    }
     // Main menu
    case x31_hash("fullscreen"): {
        fullscreen ? leave_fullscreen() : enter_fullscreen();
        break;
    }
    case x31_hash("register_as_browser"): {
        register_as_browser();
        break;
    }
    case x31_hash("open_selected_links"): {
        if (activity) {
            activity->message_to_webview(json::array("open_selected_links"));
        }
        break;
    }
    case x31_hash("quit"): {
        quit();
        break;
    }
    default: {
        throw logic_error("Unknown message name");
    }
    }

    } catch (exception& e) {
        show_string_error(__FILE__, __LINE__, (string("Uncaught exception: ") + e.what()).c_str());
        throw;
    }
}

///// VIEW STUFF

struct WindowUpdater : Observer {
    void Observer_after_commit (
        const Update& update
    ) override {
        for (ViewID v : update.views) {
            ViewData view = v.load();
            auto iter = open_windows.find(v);
            Window* window = iter == open_windows.end() ? nullptr : iter->second;
            if (view.exists && !view.closed_at) {
                if (window) {
                    window->send_update(update);
                }
                else {
                    window = new Window (v);
                     // TODO: auto load focused tab
                }
            }
            else if (window) {
                LOG("Destroying window", v);
                AW(DestroyWindow(window->os_window.hwnd));
            }
        }
    }
};
static WindowUpdater window_updater;

 // Must match constants in shell.js
enum TabFlags {
    EXISTS = 1,
    FOCUSED = 2,
    VISITED = 4,
    LOADING = 8,
    LOADED = 16,
    TRASHED = 32,
    EXPANDED = 64,
};

static json::Array make_tab_json (Window& w, const Tab& tab) {
    LinkData link;
    if (tab.id) link = tab.id.load();
    PageData page = tab.page.load();

     // Choose title
    string title = page.title;
    if (title.empty()) title = link.title; // TODO: don't use this if inverted link
    if (title.empty()) title = page.url;
     // Compile flags
    uint flags = 0;
    if (link.exists) flags |= EXISTS;
    if (w.view.focused_tab == link.id) flags |= FOCUSED;
    if (page.visited_at) flags |= VISITED;
    if (Activity* activity = activity_for_page(page.id)) {
        if (activity->currently_loading) flags |= LOADING;
        else flags |= LOADED;
    }
    if (link.trashed_at) flags |= TRASHED;
    if (w.view.expanded_tabs.count(link.id)) flags |= EXPANDED;

    return json::array(
        tab.id,
        tab.id ? json::Value(tab.parent) : json::Value(nullptr),
        link.position.hex(),
        page.url,
        page.favicon_url,
        title,
        flags
    );
}

 // Send all the tabs in this view
void Window::send_view () {
    json::Array tabs;
    tabs.reserve(visible_tabs.size());
    for (auto& [id, tab] : visible_tabs) {
        tabs.emplace_back(make_tab_json(*this, tab));
    }
    message_to_shell(json::array("view", tabs));
}

 // Send only tabs that have changed
void Window::send_update (const Update& update) {
     // First update our cached view
    bool focus_changed = false;
    for (auto id : update.views) {
        if (id == view.id) {
            ViewData old = move(view);
            view = id.load();
            if (view.focused_tab != old.focused_tab) {
                focus_changed = true;
            }
            if (view.expanded_tabs != old.expanded_tabs) {
                 // Theoretically we could update the existing visible_tabs
                 //  instead of recreating it every time, but the optimization
                 //  probably isn't worth the effort.
                gen_visible_tabs(*this);
            }
            break;
        }
    }

    if (shell) {
         // Now merge all link updates and page updates into tab updates
        std::unordered_map<LinkID, Tab*> updated_tabs;
        for (LinkID l : update.links) {
            auto iter = visible_tabs.find(l);
            if (iter != visible_tabs.end()) {
                updated_tabs.emplace(l, &iter->second);
            }
        }
        for (PageID p : update.pages) {
             // Okay this is a little awkward because we don't keep an index of
             //  tabs by page ID
            for (auto& [id, tab] : visible_tabs) {
                if (tab.page == p) {
                    updated_tabs.emplace(tab.id, &tab);
                }
            }
        }

         // Now send it
        json::Array tabs;
        tabs.reserve(updated_tabs.size());
        for (auto& [id, tab] : updated_tabs) {
            tabs.emplace_back(make_tab_json(*this, *tab));
        }
        message_to_shell(json::array("update", tabs));
    }

    PageID focused_page = visible_tabs.at(view.focused_tab).page;
     // Update window title
    if (focus_changed) {
        const string& title = focused_page.load().title;
        os_window.set_title(title.empty() ? "Sequoia" : (title + " – Sequoia").c_str());
    }
     // TODO: set UI focus?
    if (!activity_for_page(focused_page)) leave_fullscreen();
}

void Window::message_to_shell (json::Value&& message) {
    if (!shell) return;
    auto s = json::stringify(message);
    LOG("message_to_shell", s);
    shell->PostWebMessageAsJson(to_utf16(s).c_str());
}
