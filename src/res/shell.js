"use strict";

let host = window.chrome.webview;

let focused_tab = null;

let $html = document.documentElement;
let $toolbar, $back, $forward, $address, $sidebar, $toplist;

$(document.body, {}, [
    $toolbar = $("div", {id:"toolbar"}, [
        $back = $("button", {id:"back"}, [], {click: e => {
            host.postMessage(["back"]);
        }}),
        $forward = $("button", {id:"forward"}, [], {click: e => {
            host.postMessage(["forward"]);
        }}),
        $address = $("input", {}, [], {keydown: e => {
            if (e.key == "Enter") {
                host.postMessage(["navigate", $address.value]);
            }
        }}),
        $("button", {id:"main-menu"}, [], {click: e => {
            let area = e.target.getBoundingClientRect();
            host.postMessage(["main_menu", area.right, area.bottom]);
        }}),
    ]),
    $sidebar = $("div", {id:"sidebar"}, [
        $toplist = $("div", {class:"list"}),
        $("div", {id:"sidebar-bottom"}, [
            $("div", {id:"resize"}, [], {mousedown:on_resize_mousedown}),
            $("div", {id:"new-tab"}, [], {click:on_new_tab_clicked}),
        ]),
    ]),
]);

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
    event.stopPropagation();
    event.preventDefault();
}
document.addEventListener("mousemove", event => {
    if (!resizing_sidebar) return;
    let new_width = sidebar_original_width - (event.clientX - sidebar_resize_origin);
    $html.style.setProperty('--sidebar-width', new_width + "px");
    send_resize();
    event.stopPropagation();
    event.preventDefault();
});
document.addEventListener("mouseup", event => {
    if (!resizing_sidebar) return;
    resizing_sidebar = false;
    event.stopPropagation();
    event.preventDefault();
});

function send_resize () {
    host.postMessage(["resize", $sidebar.offsetWidth, $toolbar.offsetHeight]);
}

function on_new_tab_clicked (event) {
    host.postMessage(["new_toplevel_tab"]);
    event.stopPropagation();
    event.preventDefault();
}


let tabs_by_id = {};

function on_tab_clicked (event) {
    let $item = event.target.closest('.item');
    if ($item) {
        if (event.button == 0) {
            host.postMessage(["focus", +$item.id]);
        }
        else if (event.button == 1) {
            host.postMessage(["close", +$item.id]);
        }
    }
    event.stopPropagation();
    event.preventDefault();
}

function on_close_clicked (event) {
    let $item = event.target.closest('.item');
    if ($item) {
        host.postMessage(["close", +$item.id]);
    }
    event.stopPropagation();
    event.preventDefault();
}

function expand_tab (tab) {
    tab.$item.classList.add("expanded");
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
    event.stopPropagation();
    event.preventDefault();
}

let commands = {
    update (updates) {
        for (let [
            id, parent, prev, next, child_count,
            title, url, loaded, closed_at, focus, can_go_back, can_go_forward
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
                                $("button", {class:"expand"}, [], {click:on_expand_clicked}),
                                $title = $("div", {class:"title"}, title),
                                $child_count = $("div", {class:"child-count"}),
                                $("button", {class:"close"}, [], {click:on_close_clicked}),
                            ], {click:on_tab_clicked, auxclick:on_tab_clicked}
                        ),
                        $list = $("div", {class:"list"}),
                    ]);

                    tab = tabs_by_id[id] = {
                        id: id,
                        parent: parent,
                        prev: prev,
                        next: next,
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
                }

                if (child_count) {
                    tab.$child_count.innerText = "+" + child_count;
                    tab.$item.classList.add("parent");
                }
                else {
                    tab.$child_count.innerText = "";
                    tab.$item.classList.remove("parent");
                }
                if (loaded) {
                    tab.$tab.classList.add("loaded");
                }
                else {
                    tab.$tab.classList.remove("loaded");
                }

                if (focus) {
                    focused_tab = tab;
                    $address.value = url;
                    $back.disabled = !can_go_back;
                    $forward.disabled = !can_go_forward;
                }
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
                console.log(tab, next);
                $parent_list.insertBefore(tab.$item, next ? next.$item : null);
                insertIfNeeded(tabs_by_id[tab.prev]);
            }
            insertIfNeeded(tabs_by_id[id]);
        }
         // Expand everything above and including the focused tab
        expandUp(focused_tab)
        function expandUp (tab) {
            if (!tab) return;
            expand_tab(tab);
            expandUp(tabs_by_id[tab.parent]);
        }
    },
    colors (toolbar_bg, toolbar_fg, tab_bg, tab_fg, tab_highlight, tab_shadow) {
        let style = document.documentElement.style;
        style.setProperty('--toolbar-bg', toolbar_bg);
        style.setProperty('--toolbar-fg', toolbar_fg);
        style.setProperty('--tab-bg', tab_bg);
        style.setProperty('--tab-fg', tab_fg);
        style.setProperty('--tab-highlight', tab_highlight);
        style.setProperty('--tab-shadow', tab_shadow);
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
