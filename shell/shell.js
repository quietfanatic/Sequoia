"use strict";

(function(){

let host = window.chrome.webview;

let focused_id = null;

let $back;
let $forward;
let $address;
let $tree;

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
        $("button", {}, "..."),
    ]),
    $tree = $("nav", {id:"tree", class:"list"}),
]);

let tabs_by_id = {};

function on_close_clicked (event) {
}

let commands = {
    update (updates) {
        for (let [
            id, parent, next, prev, child_count,
            title, url, focus, can_go_back, can_go_forward
        ] of updates) {
            let tooltip = title;
            if (url) tooltip += "\n" + url;
            if (child_count > 1) tooltip += "\n(" + child_count + ")";

            let tab = tabs_by_id[id];
            if (tab) {
                if (parent != tab.parent || next != tab.next) {
                    throw "TODO: move tab";
                }
                tab.$tab.setAttribute("title", tooltip);
                tab.$title.innerText = title;
            }
            else {
                let $title = $("div", {class:"title"}, title);
                let $close = $("button", {class:"close"}, "✗", {click:on_close_clicked})
                let $tab = $("div", {class:"tab", title:tooltip}, [$title, $close]);
                let $list = $("div", {class:"list"});
                let $item = $("div", {class:"item"}, [$tab, $list]);

                tabs_by_id[id] = {
                    $item: $item,
                    $tab: $tab,
                    $title: $title,
                    $list: $list,
                    parent: 0+parent,
                    next: 0+next
                };

                let $parent_list = parent == 0 ? $tree : tabs_by_id[parent].$list;
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
    },
};

host.addEventListener("message", e=>{
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
