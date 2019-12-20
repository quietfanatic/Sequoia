"use strict";

let host = window.chrome.webview;

let focused_id = null;

let $back;
let $forward;
let $address;
let $toplist;

$(document.body, {}, [
    $("nav", {id:"toolbar"}, [
        $back = $("button", {}, "<", {click: e => {
            host.postMessage(["back"]);
        }}),
        $forward = $("button", {}, ">", {click: e => {
            host.postMessage(["forward"]);
        }}),
        $address = $("input", {}, [], {keydown: e => {
            if (e.key == "Enter") {
                host.postMessage(["navigate", $address.value]);
            }
        }}),
        $("button", {}, "⋯", {click: e => {
            let area = e.target.getBoundingClientRect();
            host.postMessage(["main_menu", area.right, area.bottom]);
        }}),
    ]),
    $("nav", {id:"tree"}, [
        $toplist = $("div", {class:"list"}),
    ]),
]);

var tabs_by_id = {};

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
                if (tab) {  // Update existing tab
                    if (parent != tab.parent || prev != tab.prev || next != tab.next) {
                        tab.$item.remove();
                    }
                    tab.parent = parent;
                    tab.prev = prev;
                    tab.next = next;
                    tab.$tab.setAttribute("title", tooltip);
                    tab.$title.innerText = title;
                    if (loaded) {
                        tab.$tab.classList.add("loaded");
                    }
                    else {
                        tab.$tab.classList.remove("loaded");
                    }
                }
                else {  // Add new tab
                    let $title = $("div", {class:"title"}, title);
                    let $close = $("button", {}, "✗", {click:on_close_clicked})
                    let $tab = $("div",
                        {class:"tab" + (loaded ? " loaded" : ""), title:tooltip},
                        [$title, $close],
                        {click:on_tab_clicked, auxclick:on_tab_clicked}
                    );
                    let $list = $("div", {class:"list"});
                    let $item = $("div", {class:"item",id:id}, [$tab, $list]);

                    tabs_by_id[id] = {
                        id: id,
                        parent: parent,
                        prev: prev,
                        next: next,
                        $item: $item,
                        $tab: $tab,
                        $title: $title,
                        $list: $list,
                    };

                }
                if (focus) {
                    focused_id = id;
                    $address.value = url;
                    $back.enabled = can_go_back;
                    $forward.enabled = can_go_forward;
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
