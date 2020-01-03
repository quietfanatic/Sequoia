"use strict";

let host = window.chrome.webview;

let focused_id = 0;
let showing_sidebar = true;
let showing_main_menu = false;
let currently_loading = false;

let $html = document.documentElement;
let $toolbar;
let $back, $forward, $reload, $address, $error_indicator, $toggle_sidebar;
let $sidebar, $toplist, $main_menu;

function handled (event) {
    event.stopPropagation();
    event.preventDefault();
}

$(document.body,
    $toolbar = $("div", {id:"toolbar"},
        $back = $("div", {
            id: "back",
            title: "Back",
            click: e => {
                host.postMessage(["back"]);
                handled(e);
            },
        }),
        $forward = $("div", {
            id: "forward",
            title: "Forward",
            click: e => {
                host.postMessage(["forward"]);
                handled(e);
            },
        }),
        $reload = $("div", {
            id: "reload",
            title: "Reload",
            click: e => {
                if (currently_loading) {
                    host.postMessage(["stop"]);
                }
                else {
                    host.postMessage(["reload"]);
                }
                handled(e);
            },
        }),
        $address = $("input", {
            id: "address",
            spellcheck: "false",
            autocapitalize: "off",
            inputmode: "url",
            placeholder: "Search or enter web address",
            keydown: e => {
                if (e.key == "Enter") {
                    host.postMessage(["navigate", $address.value]);
                    handled(e);
                }
            },
        }),
        $error_indicator = $("div", {
            id: "error-indicator",
            title: "An error has occured in the Sequoia shell.\n"
                 + "Click here to investigate with the DevTools.",
            click: e => {
                $error_indicator.classList.remove("has-error");
                host.postMessage(["investigate_error"]);
            },
        }),
        $toggle_sidebar = $("div", {
            id: "toggle-sidebar",
            title: "Hide sidebar",
            click: e => {
                showing_sidebar = !showing_sidebar;
                $html.classList.toggle("hide-sidebar", !showing_sidebar);
                $toggle_sidebar.title = showing_sidebar ? "Hide sidebar" : "Show sidebar";
                send_resize();
                handled(e);
            },
        }),
        $("div", {
            id: "show-main-menu",
            title: "Main menu",
            click: e => {
                if (showing_main_menu) {
                    close_main_menu();
                }
                else {
                    open_main_menu();
                }
                handled(e);
            },
        }),
    ),
    $sidebar = $("div", {id:"sidebar"},
        $("div", {id:"tree"},
            $toplist = $("div", {class:"list"}),
            $("div", {id:"new-tab", click:on_new_tab_clicked}),
        ),
        $("div", {id:"sidebar-bottom"},
            $("div", {
                id: "resize",
                title: "Resize sidebar",
                mousedown: on_resize_mousedown
            }),
            $("div", {
                id: "show-closed",
                title: "Show closed tabs",
                click: e => {
                    $html.classList.toggle("show-closed");
                },
            }),
        ),
    ),
    $main_menu = $("nav", {id:"main-menu"},
        $("div", "New Toplevel Tab", {
            click: menu_item("new_toplevel_tab"),
        }),
        $("div", "Fix database problems", {
            click: menu_item("fix_problems"),
        }),
        $("div", "Quit", {
            click: menu_item("quit"),
        }),
    ),
);

window.addEventListener("error", e => {
    $error_indicator.classList.add("has-error");
});

let $tab_destination_marker = $("div", {class:"tab-destination-marker"});

function menu_item (message) {
    return event => {
        host.postMessage([message]);
        close_main_menu();
        handled(event);
    };
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

function on_resize_mousedown (event) {
    let $resize = event.target.closest('#resize');
    if (!$resize) return;
    resizing_sidebar = true;
    sidebar_resize_origin = event.clientX;
    sidebar_original_width = $sidebar.offsetWidth;
    document.addEventListener("mousemove", on_resize_drag);
    document.addEventListener("mouseup", on_resize_release);
    handled(event);
}
function on_resize_drag (event) {
    let new_width = sidebar_original_width - (event.clientX - sidebar_resize_origin);
    $html.style.setProperty('--sidebar-width', new_width + "px");
    send_resize();
    handled(event);
}
function on_resize_release (event) {
    resizing_sidebar = false;
    document.removeEventListener("mousemove", on_resize_drag);
    document.removeEventListener("mouseup", on_resize_release);
    handled(event);
}

let grabbing_tab = false;
let moving_tab = false;
let moving_tab_id = 0;
let $move_destination = null;
let move_destination_rel = 0;
let tab_move_origin_y = 0;

let TabRelation = {
    BEFORE: 0,
    AFTER: 1,
    FIRST_CHILD: 2,
    LAST_CHILD: 3
};

function on_tab_mousedown (event) {
    let $item = event.target.closest('.item');
    if (!$item) return;
    grabbing_tab = true;
    tab_move_origin_y = event.clientY;
    document.addEventListener("mousemove", on_tab_drag);
    document.addEventListener("mouseup", on_tab_release);
    moving_tab_id = +$item.id;
    handled(event);
}
function on_tab_drag (event) {
    let diff = event.clientY - tab_move_origin_y;
    if (diff*diff > 24*24) {
        moving_tab = true;
        $html.classList.add("moving-tab");
    }
    if (moving_tab) {
        $move_destination = event.target.closest('.tab');
        if ($move_destination) {
            let bounds = $move_destination.getBoundingClientRect();
            let ratio = (event.clientY - bounds.top) / bounds.height;
            $move_destination.appendChild($tab_destination_marker);
            if (ratio < 0.3) {
                move_destination_rel = TabRelation.BEFORE;
                $tab_destination_marker.classList.add("before");
                $tab_destination_marker.classList.remove("after");
                $tab_destination_marker.classList.remove("inside");
            }
            else if (ratio > 0.7) {
                move_destination_rel = TabRelation.AFTER;
                $tab_destination_marker.classList.remove("before");
                $tab_destination_marker.classList.add("after");
                $tab_destination_marker.classList.remove("inside");
            }
            else {
                move_destination_rel = TabRelation.LAST_CHILD;
                $tab_destination_marker.classList.remove("before");
                $tab_destination_marker.classList.remove("after");
                $tab_destination_marker.classList.add("inside");
            }
        }
        else {
            $tab_destination_marker.remove();
        }
    }
    handled(event);
}
function on_tab_release (event) {
    grabbing_tab = false;
    $tab_destination_marker.remove();
    $html.classList.remove("moving-tab");
    document.removeEventListener("mousemove", on_tab_drag);
    document.removeEventListener("mouseup", on_tab_release);
    if (moving_tab) {
        moving_tab = false;
        if ($move_destination) {
            let $item = $move_destination.closest('.item');
            if (+$item.id != moving_tab_id) {
                host.postMessage(["move_tab", moving_tab_id, +$item.id, move_destination_rel]);
            }
        }
    }
    handled(event);
}

function send_resize () {
    let x = showing_sidebar ? $sidebar.offsetWidth : 0;
    let y = $toolbar.offsetHeight;
    host.postMessage(["resize", x, $toolbar.offsetHeight]);
}

function on_new_tab_clicked (event) {
    host.postMessage(["new_toplevel_tab"]);
    handled(event);
}

 // Prevent autoscroll behavior
document.addEventListener("mousedown", event => {
    if (event.button == 1) {
        handled(event);
    }
});

let tabs_by_id = {};

function close_or_delete_item ($item) {
    if ($item.classList.contains("closed")) {
        host.postMessage(["delete", +$item.id]);
    }
    else {
        host.postMessage(["close", +$item.id]);
    }
}

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
            tabs_by_id[id].$tab.focus();
        }
        else if (event.button == 1) {
            close_or_delete_item($item);
        }
    }
    handled(event);
}

function on_new_child_clicked (event) {
    let $item = event.target.closest('.item');
    if ($item) {
        host.postMessage(["new_child", +$item.id]);
    }
    handled(event);
}

function on_close_clicked (event) {
    let $item = event.target.closest('.item');
    if ($item) {
        close_or_delete_item($item);
    }
    handled(event);
}

function expand_tab (tab) {
    tab.$item.classList.add("expanded");
    let depth = 0;
    let t = tab;
    while (t = tabs_by_id[t.parent]) {
        depth += 1;
    }
    tab.$list.classList.toggle("alt", depth % 4 >= 2);
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

function set_address (url) {
    $address.value = (url == "about:blank" ? "" : url);
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
            set_address(tab.url);
             // Expand everything above and including the focused tab
            expandUp(tab)
            function expandUp (tab) {
                if (!tab) return;
                expand_tab(tab);
                expandUp(tabs_by_id[tab.parent]);
            }
            if (tab.url == "about:blank") {
                $address.focus();
            }
        }
    },
    activity (can_go_back, can_go_forward, loading) {
        $back.classList.toggle("disabled", !can_go_back);
        $forward.classList.toggle("disabled", !can_go_forward);
        currently_loading = loading;
        $reload.classList.toggle("loading", loading);
        $reload.title = currently_loading ? "Stop" : "Reload";
    },
    tabs (updates) {
        for (let [
            id, parent, position, child_count, url, title, favicon, loaded, visited_at, closed_at
        ] of updates) {
            if (parent === undefined) {
                let tab = tabs_by_id[id];
                if (tab) {
                    tab.$item.remove();
                }
                delete tabs_by_id[id];
                continue;
            }
            let tooltip = title;
            if (url) tooltip += "\n" + url;
            if (child_count > 1) tooltip += "\n(" + child_count + ")";

            if (!title) title = url;

            let tab = tabs_by_id[id];
            if (!tab) {  // Create new tab
                let $item, $tab, $favicon, $title, $child_count, $list;
                $item = $("div", {
                    id: id,
                    class: "item",
                }, [
                    $tab = $("div", {
                        class: "tab",
                        title: tooltip,
                        tabIndex: "0",
                        click: on_tab_clicked,
                        auxclick: on_tab_clicked,
                        mousedown: on_tab_mousedown,
                    }, [
                        $("div", {
                            class: "expand",
                            click: on_expand_clicked,
                        }),
                        $favicon = $("img", {class:"favicon"}),
                        $title = $("div", {class:"title"}, title),
                        $child_count = $("div", {class:"child-count"}),
                        $("div", {
                            class: "new-child",
                            click: on_new_child_clicked,
                        }),
                        $("div", {
                            class: "close",
                            click: on_close_clicked,
                        }),
                    ]),
                    $list = $("div", {class:"list"}),
                ]);

                tab = tabs_by_id[id] = {
                    id: id,
                    parent: parent,
                    position: position,
                    url: url,
                    expanded: false,
                    $item: $item,
                    $tab: $tab,
                    $favicon: $favicon,
                    $title: $title,
                    $child_count: $child_count,
                    $list: $list,
                };
            }
            else {  // Update existing tab
                if (parent != tab.parent || position != tab.position) {
                    tab.$item.remove();
                }
                tab.parent = parent;
                tab.position = position;
                tab.url = url;
                tab.$tab.setAttribute("title", tooltip);
                tab.$title.innerText = title;
                if (focused_id == id) {
                    set_address(tab.url);
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
            tab.$tab.classList.toggle("visited", visited_at > 0);
            tab.$item.classList.toggle("closed", closed_at > 0);
            if (favicon) tab.$favicon.src = favicon;
            else tab.$favicon.removeAttribute("src");
        }
         // Wait until we're done updating to insert moved tabs, to make sure
         //   parent tabs have been delivered.
        for (let [id] of updates) {
            let tab = tabs_by_id[id];
            if (!tab) continue;  // Must have been deleted
            if (tab.$item.isConnected) continue;
            let $parent_list = tab.parent == 0 ? $toplist : tabs_by_id[tab.parent].$list;
             // Insert sorted by position.
            for (let $child_item of $parent_list.childNodes) {
                let child = tabs_by_id[+$child_item.id];
                if (tab.position < child.position) {
                    $parent_list.insertBefore(tab.$item, $child_item);
                    break;
                }
            }
            if (!tab.$item.isConnected) {
                $parent_list.appendChild(tab.$item);
            }
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
    if (e.data.length < 1) {
        console.log("Shell received unrecognized message format");
        return;
    }
    let command = e.data.shift();
    console.log(command, e.data);
    if (command in commands) {
        commands[command].apply(undefined, e.data)
    }
    else {
        console.log("Shell received unknown message: ", command);
    }
});

host.postMessage(["ready"]);
send_resize();
