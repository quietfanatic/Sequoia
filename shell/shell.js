"use strict";

(function(){

let host = window.chrome.webview;

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

let items_by_id = {};

function on_close_clicked (event) {
}

let commands = {
    add_tab (id, parent, next, prev, child_count, title) {
        console.log(0);
        let $tab = $("div", {class:"tab"}, [
            child_count > 1 ? title + " (" + child_count + ")" : title,
            $("button", {}, "✗", {click:on_close_clicked})
        ]);
        let $list = $("div", {class:"list"});
        let $item = $("div", {class:"item"}, [$tab, $list]);
        items_by_id[id] = {
            $item: $item,
            $tab: $tab,
            $list: $list
        };

        let $parent_list = parent == 0 ? $tree : items_by_id[parent].$list;

        let $next = next == 0 ? null : tabs_by_id[next].$item;
        $parent_list.insertBefore($item, $next);
    },
    update_tab (id, parent, next, prev, child_count, title) {
    },
    remove_tab (id) {
    },
    update (url, back, forward) {
        $address.value = url;
        $back.enabled = back;
        $forward.enabled = forward;
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
        console.log("Shell received unknown message");
    }
});

host.postMessage(["ready"]);

})();
