"use strict";

let host = window.chrome.webview;

let focused_id = 0;
let showing_sidebar = true;
let showing_main_menu = false;
let currently_loading = false;

let $html = document.documentElement;
let $toolbar, $back, $forward, $reload, $address, $sidebar, $toplist, $main_menu;

function handled (event) {
    event.stopPropagation();
    event.preventDefault();
}

$(document.body, {}, [
    $toolbar = $("div", {id:"toolbar"}, [
        $back = $("div", {id:"back"}, [], {click: e => {
            host.postMessage(["back"]);
            handled(e);
        }}),
        $forward = $("div", {id:"forward"}, [], {click: e => {
            host.postMessage(["forward"]);
            handled(e);
        }}),
        $reload = $("div", {id:"reload"}, [], {click:reload_or_stop}),
        $address = $("input", {}, [], {keydown: e => {
            if (e.key == "Enter") {
                host.postMessage(["navigate", $address.value]);
                handled(e);
            }
        }}),
        $("div", {id:"toggle-sidebar"}, [], {click: e => {
            showing_sidebar = !showing_sidebar;
            $html.classList.toggle("hide-sidebar", showing_sidebar);
            send_resize();
            handled(e);
        }}),
        $("div", {id:"show-main-menu"}, [], {click: e => {
            if (showing_main_menu) {
                close_main_menu();
            }
            else {
                open_main_menu();
            }
            handled(e);
        }}),
    ]),
    $sidebar = $("div", {id:"sidebar"}, [
        $toplist = $("div", {class:"list"}),
        $("div", {id:"sidebar-bottom"}, [
            $("div", {id:"resize"}, [], {mousedown:on_resize_mousedown}),
            $("div", {id:"new-tab"}, [], {click:on_new_tab_clicked}),
        ]),
    ]),
    $main_menu = $("nav", {id:"main-menu"}, [
        $("div", {id:"new-toplevel-tab"}, "New Toplevel Tab", {
            click: menu_item("new_toplevel_tab"),
        }),
        $("div", {id:"quit"}, "Quit", {
            click: menu_item("quit"),
        }),
    ]),
]);
function menu_item(message) {
    return event => {
        host.postMessage([message]);
        close_main_menu();
        handled(event);
    };
}

function reload_or_stop (event) {
    if (currently_loading) {
        host.postMessage(["stop"]);
    }
    else {
        host.postMessage(["reload"]);
    }
    handled(event);
}

document.addEventListener("click", e => {
    let $show_main_menu = event.target.closest("#show-main-menu");
    if ($show_main_menu) return;
    close_main_menu();
}, {capture:true});
window.addEventListener("blur", e => {
    close_main_menu();
    handled(e);
});

function open_main_menu () {
    showing_main_menu = true;
    $html.classList.add("show-main-menu");
    host.postMessage(["show_main_menu", $main_menu.offsetWidth]);
}

function close_main_menu () {
    showing_main_menu = false;
    $html.classList.remove("show-main-menu");
    host.postMessage(["hide_main_menu"]);
}

let resizing_sidebar = false;
let sidebar_resize_origin = 0;
let sidebar_original_width = 0;

function on_resize_mousedown(event) {
     // Start resize if we're clicking the sidebar but not a tab
    let $tab = event.target.closest('.tab');
    if ($tab) return;
    let $resize = event.target.closest('#resize');
    if (!$resize) return;
    resizing_sidebar = true;
    sidebar_resize_origin = event.clientX;
    sidebar_original_width = $sidebar.offsetWidth;
    handled(event);
}
document.addEventListener("mousemove", event => {
    if (!resizing_sidebar) return;
    let new_width = sidebar_original_width - (event.clientX - sidebar_resize_origin);
    $html.style.setProperty('--sidebar-width', new_width + "px");
    send_resize();
    handled(event);
});
document.addEventListener("mouseup", event => {
    if (!resizing_sidebar) return;
    resizing_sidebar = false;
    handled(event);
});

function send_resize () {
    let x = showing_sidebar ? $sidebar.offsetWidth : 0;
    let y = $toolbar.offsetHeight;
    host.postMessage(["resize", x, $toolbar.offsetHeight]);
}

function on_new_tab_clicked (event) {
    host.postMessage(["new_toplevel_tab"]);
    handled(event);
}

document.addEventListener("mousedown", event => {
    if (event.button == 1) {
        handled(event);
    }
});

let tabs_by_id = {};

function on_tab_clicked (event) {
    let $item = event.target.closest('.item');
    if ($item) {
        let id = +$item.id;
        if (event.button == 0) {
            if (focused_id != id) {
                host.postMessage(["focus", id]);
            }
            else if (!tabs_by_id[id].loaded) {
                host.postMessage(["load", id]);
            }
        }
        else if (event.button == 1) {
            host.postMessage(["close", id]);
        }
    }
    handled(event);
}

function on_close_clicked (event) {
    let $item = event.target.closest('.item');
    if ($item) {
        host.postMessage(["close", +$item.id]);
    }
    handled(event);
}

function expand_tab (tab) {
    tab.$item.classList.add("expanded");
    let depth = 0;
    for (let t = tab; t; t = tabs_by_id[t.parent]) {
        depth += 1;
    }
    tab.$list.classList.toggle("alt", !(depth % 2));
    tab.expanded = true;
}
function contract_tab (tab) {
    tab.$item.classList.remove("expanded");
    tab.expanded = false;
}
function on_expand_clicked (event) {
    let $item = event.target.closest('.item');
    if ($item) {
        let tab = tabs_by_id[+$item.id];
        if (tab.expanded) {
            contract_tab(tab);
        }
        else {
            expand_tab(tab);
        }
    }
    handled(event);
}

let commands = {
    focus (id) {
        let old_tab = tabs_by_id[focused_id];
        if (old_tab) {
            old_tab.$tab.classList.remove("focused");
        }
        focused_id = id;
        let tab = tabs_by_id[id];
        if (tab) {
            tab.$tab.classList.add("focused");
            $address.value = tab.url;
             // Expand everything above and including the focused tab
            expandUp(tab)
            function expandUp (tab) {
                if (!tab) return;
                expand_tab(tab);
                expandUp(tabs_by_id[tab.parent]);
            }
        }
    },
    activity (can_go_back, can_go_forward, loading) {
        $back.classList.toggle("disabled", !can_go_back);
        $forward.classList.toggle("disabled", !can_go_forward);
        currently_loading = loading;
        $reload.classList.toggle("loading", loading);
    },
    tabs (updates) {
        for (let [
            id, parent, prev, next, child_count, title, url, loaded, closed_at
        ] of updates) {
            id = +id;
            parent = +parent;
            prev = +prev;
            next = +next;
            child_count = +child_count;
            if (closed_at != 0) {  // Remove tab
                let tab = tabs_by_id[id];
                if (tab) {
                    tab.$item.remove();
                    delete tabs_by_id[id];
                }
            }
            else {
                let tooltip = title;
                if (url) tooltip += "\n" + url;
                if (child_count > 1) tooltip += "\n(" + child_count + ")";

                if (!title) title = url;

                let tab = tabs_by_id[id];
                if (!tab) {  // Create new tab
                    let $item, $tab, $title, $child_count, $list;
                    $item = $("div", {id:id,class:"item"}, [
                        $tab = $("div",
                            {class:"tab", title:tooltip}, [
                                $("div", {class:"expand"}, [], {click:on_expand_clicked}),
                                $title = $("div", {class:"title"}, title),
                                $child_count = $("div", {class:"child-count"}),
                                $("div", {class:"close"}, [], {click:on_close_clicked}),
                            ], {click:on_tab_clicked, auxclick:on_tab_clicked}
                        ),
                        $list = $("div", {class:"list"}),
                    ]);

                    tab = tabs_by_id[id] = {
                        id: id,
                        parent: parent,
                        prev: prev,
                        next: next,
                        url: url,
                        expanded: false,
                        $item: $item,
                        $tab: $tab,
                        $title: $title,
                        $child_count: $child_count,
                        $list: $list,
                    };
                }
                else {  // Update existing tab
                    if (parent != tab.parent || prev != tab.prev || next != tab.next) {
                        tab.$item.remove();
                    }
                    tab.parent = parent;
                    tab.prev = prev;
                    tab.next = next;
                    tab.$tab.setAttribute("title", tooltip);
                    tab.$title.innerText = title;
                    if (focused_id == id) {
                        $address.value = tab.url;
                    }
                }

                if (child_count) {
                    tab.$child_count.innerText = "(" + child_count + ")";
                    tab.$item.classList.add("parent");
                }
                else {
                    tab.$child_count.innerText = "";
                    tab.$item.classList.remove("parent");
                }
                tab.$tab.classList.toggle("loaded", loaded);

            }
        }
         // Wait until we're done updating to insert moved tabs, so that all new
         // prev and next ids are reflected.
        for (let [id] of updates) {
            id = +id;
            function insertIfNeeded (tab) {
                if (!tab) return;
                if (tab.$item.isConnected) return;
                let next = tab.next == 0 ? null : tabs_by_id[tab.next];
                if (next && !next.$item.isConnected) return;
                let $parent_list = tab.parent == 0 ? $toplist : tabs_by_id[tab.parent].$list;
                $parent_list.insertBefore(tab.$item, next ? next.$item : null);
                insertIfNeeded(tabs_by_id[tab.prev]);
            }
            insertIfNeeded(tabs_by_id[id]);
        }
    },
    settings (settings) {
        if ("theme" in settings) {
            for (let token of $html.classList) {
                if (token.startsWith("theme-")) {
                    $html.classList.remove(token);
                    break;
                }
            }
            $html.classList.add("theme-" + settings.theme);
        }
    }
};

host.addEventListener("message", e=>{
    console.log(e.data);
    if (e.data.length < 1) {
        console.log("Shell received unrecognized message format");
        return;
    }
    let command = e.data.shift();
    console.log(command);
    if (command in commands) {
        commands[command].apply(undefined, e.data)
    }
    else {
        console.log("Shell received unknown message: ", command);
    }
});

host.postMessage(["ready"]);
send_resize();
