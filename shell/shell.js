"use strict";

(function(){

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
            host.postMessage(["main_menu"]);
        }}),
    ]),
    $("nav", {id:"tree"}, [
        $toplist = $("_list"),
    ]),
]);

let tabs_by_id = {};

function get_ancestor ($node, f) {
    while ($node && !f($node)) {
        $node = $node.parentNode;
    }
    return $node;
}

function on_close_clicked (event) {
    console.log(event);
    let $item = get_ancestor(event.target, $ => $.nodeName == '_ITEM');
    console.log("Closing: ", $item);
    if ($item) {
        host.postMessage(["close", +$item.id]);
    }
}

let commands = {
    update (updates) {
        for (let [
            id, parent, next, prev, child_count,
            title, url, focus, can_go_back, can_go_forward
        ] of updates) {
            if (parent == -9) {  // Tab::DELETE
                let tab = tabs_by_id[id];
                if (tab) {
                    tab.$item.parentNode.removeChild(tab.$item);
                    delete tabs_by_id[id];
                }
            }
            else {
                let tooltip = title;
                if (url) tooltip += "\n" + url;
                if (child_count > 1) tooltip += "\n(" + child_count + ")";

                let tab = tabs_by_id[id];
                if (tab) {  // Update existing tab
                    if (parent != tab.parent || next != tab.next) {
                        throw "TODO: move tab";
                    }
                    tab.$tab.setAttribute("title", tooltip);
                    tab.$title.innerText = title;
                }
                else {  // Add new tab
                    let $title = $("_title", {}, title);
                    let $close = $("button", {}, "✗", {click:on_close_clicked})
                    let $tab = $("_tab", {title:tooltip}, [$title, $close]);
                    let $list = $("_list", {});
                    let $item = $("_item", {id:id}, [$tab, $list]);

                    tabs_by_id[id] = {
                        $item: $item,
                        $tab: $tab,
                        $title: $title,
                        $list: $list,
                        parent: +parent,
                        next: +next
                    };

                    let $parent_list = parent == 0 ? $toplist : tabs_by_id[parent].$list;
                    let $next = next == 0 ? null : tabs_by_id[next].$item;
                    $parent_list.insertBefore($item, $next);

                }
                if (focus) {
                    focused_id = id;
                    $address.value = url;
                    $back.enabled = can_go_back;
                    $forward.enabled = can_go_forward;
                }
            }
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
    if (command in commands) {
        commands[command].apply(undefined, e.data)
    }
    else {
        console.log("Shell received unknown message: ", command);
    }
});

host.postMessage(["ready"]);

})();
