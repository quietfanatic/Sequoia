"use strict";

let host = window.chrome.webview;
let $html = document.documentElement;

 // Must match enum ShellItemFlags in Window.cpp
const FOCUSED = 1;
const VISITED = 2;
const LOADING = 4;
const LOADED = 8;
const TRASHED = 16;
const EXPANDABLE = 32;
const EXPANDED = 64;

///// View model (model of view, not viewmodel)

 // Although the data model at the bottom is a graph of pages and links, it's
 // presented to the user as a tree of tabs, which looks in the DOM like
 //     [Item [Tab] [List
 //         [Item [Tab]]
 //         [Item [Tab] [List
 //             [Item [Tab]]
 //         ]
 //     ]
 // The conversion is done on the app side, and communicated to this shell as
 // changes to this tree of tabs.  Most of the data is just stuffed directly
 // into the DOM without being stored in JS.  The only JS data stored per item
 // is for positioning.

///// Toolbar DOM

 // TODO: this will all be replaced with a navigation page.

 // Simple back and forward buttons
let $back = $("div", {
    id: "back",
    title: "Back",
    click: e => {
        host.postMessage(["back"]);
        handled(e);
    },
});
let $forward = $("div", {
    id: "forward",
    title: "Forward",
    click: e => {
        host.postMessage(["forward"]);
        handled(e);
    },
});

 // Reload and stop buttons.  Only one will be shown at a time.
let $reload = $("div", {
    id: "reload",
    title: "Reload",
    click: e => {
        host.postMessage(["reload"]);
        handled(e);
    },
});
let $stop = $("div", {
    id: "stop",
    title: "Stop",
    click: e => {
        host.postMessage(["stop"]);
        handled(e);
    },
});

 // Address bar is surprisingly simple, at least for now
let $address = $("input", {
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
});
function set_address (url) {
    $address.value = (url == "about:blank" ? "" : url);
}

 // Hopefully this never appears except during development
let $error_indicator = $("div", {
    id: "error-indicator",
    title: "An error has occured in the Sequoia shell.\n"
         + "Click here to investigate with the DevTools.",
    click: e => {
        $error_indicator.classList.remove("has-error");
        host.postMessage(["investigate_error"]);
    },
});
window.addEventListener("error", e => {
    $error_indicator.classList.add("has-error");
});

 // Button to show and hide the sidebar.
 // I don't actually use this much now that I have fullscreen.
let showing_sidebar = true;
let $toggle_sidebar = $("div", {
    id: "toggle-sidebar",
    title: "Hide sidebar",
    click: e => {
        showing_sidebar = !showing_sidebar;
        $html.classList.toggle("hide-sidebar", !showing_sidebar);
        $toggle_sidebar.title = showing_sidebar ? "Hide sidebar" : "Show sidebar";
        send_resize();
        handled(e);
    },
});

 // Show/hide the main menu.  Not entirely satisfied.
let showing_main_menu = false;
let $toggle_main_menu = $("div", {
    id: "toggle-main-menu",
    title: "Main menu",
    click: e => {
        if (showing_main_menu) {
            hide_main_menu();
        }
        else {
            show_main_menu();
        }
        handled(e);
    },
});

 // Put it all together
let $toolbar = $("div", {id:"toolbar"},
    $back, $forward, $reload, $stop, $address,
    $error_indicator, $toggle_sidebar, $toggle_main_menu
);

function update_toolbar (url, loading) {
    $reload.classList.toggle("disabled", loading)
    $stop.classList.toggle("disabled", !loading)
    $html.classList.toggle("currently-loading", loading);
    set_address(url);
}

///// Sidebar DOM

let $toplist = $("div", {class:"list"});
let $sidebar = $("div", {id:"sidebar"},
    $("div", {id:"tree"},
        $toplist
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
);

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
    let new_width = sidebar_original_width - (
        event.clientX - sidebar_resize_origin
    );
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

///// Main menu DOM

let $main_menu = $("nav", {id:"main-menu"},
    $("div", "Enter fullscreen", {
        click: menu_item("fullscreen"),
    }),
    $("div", "Register as browser with Windows", {
        click: menu_item("register_as_browser"),
    }),
    $("div", "Open selected links in tabs", {
        click: menu_item("open_selected_links"),
    }),
    $("div", "Quit", {
        click: menu_item("quit"),
    }),
     // Don't hide menu when clicking it.
    {click: e=>{ handled(e); }},
);

function menu_item (message) {
    return event => {
        host.postMessage([message]);
        hide_main_menu();
        handled(event);
    };
}

function show_main_menu () {
    if (showing_main_menu) return;
    showing_main_menu = true;
    $html.classList.add("show-main-menu");
    host.postMessage(["show_menu", $main_menu.offsetWidth]);
}

function hide_main_menu () {
    if (!showing_main_menu) return;
    showing_main_menu = false;
    $html.classList.remove("show-main-menu");
    host.postMessage(["hide_menu"]);
}

 // Hide menus if you click outside of them
//document.addEventListener("click", e => {
//    hide_main_menu();
//    hide_context_menu();
//    // Let event propagate
//}, {capture:true});

 // Hide menus if the shell loses focus
window.addEventListener("blur", e => {
    hide_main_menu();
    // Let event propogate
}, {capture:true});

///// Tab DOM and behavior

let items_by_id = {};

function create_item ([id, parent, position, url, favicon_url, title, flags]) {
    let classes = "item";
    if (flags & FOCUSED) classes += " focused";
    if (flags & VISITED) classes += " visited";
    if (flags & LOADED) classes += " loaded";
    if (flags & TRASHED) classes += " trashed";
    if (flags & EXPANDABLE) classes += " expandable";
    if (flags & EXPANDED) classes += " expanded";
    if (flags & FOCUSED) update_toolbar(url, flags & LOADING);
    let $item, $tab, $favicon, $title, $list;
    $item = $("div",
        {
            id: id,
            class: classes,
        },
        $tab = $("div",
            {
                class: "tab",
                title: title + "\n" + url,
                tabIndex: "0",
                click: on_tab_clicked,
                auxclick: on_tab_clicked,
                mousedown: on_tab_mousedown,
            },
            $("div", {
                class: "expand",
                click: on_expand_clicked,
            }),
            $favicon = $("img", {
                class: "favicon",
                src: favicon_url,
                load: e => e.target.classList.add("loaded"),
                error: e => e.target.classList.remove("loaded"),
            }),
            $title = $("div", {class:"title"}, title),
            $("div", {
                class: "close",
                click: on_close_clicked,
            }),
            $("div", {
                class: "new-child",
                click: on_new_child_clicked,
            }),
        ),
        $list = $("div", {class:"list"}),
    );
    return items_by_id[id] = {
        parent: null,  // Will be placed later
        position: null,
        $item: $item,
        $tab: $tab,
        $favicon: $favicon,
        $title: $title,
        $list: $list
    };
}

function update_item (data) {
    let [id, parent, position, url, favicon_url, title, flags] = data;
    let item = items_by_id[id];
    if (!item) {
        create_item(data);
    }
    else {
        item.$item.classList.toggle("focused", flags & FOCUSED);
        item.$item.classList.toggle("visited", flags & VISITED);
        item.$item.classList.toggle("loading", flags & LOADING);
        item.$item.classList.toggle("loaded", flags & LOADED);
        item.$item.classList.toggle("trashed", flags & TRASHED);
        item.$item.classList.toggle("expandable", flags & EXPANDABLE);
        item.$item.classList.toggle("expanded", flags & EXPANDED);
        if (flags & FOCUSED) update_toolbar(url, flags & LOADING);

        item.$title.innerText = title;
        item.$tab.title = title + "\n" + url;

        if (favicon_url) {
            item.$favicon.src = favicon_url;
        }
        else {
            item.$favicon.removeAttribute("src");
        }

        if (flags & FOCUSED) update_toolbar(url, flags & LOADING);

        if (parent != item.parent || position != item.position) {
            $item.remove(); // Will be reinserted later
        }
    }
}

function remove_item (id) {
    items_by_id[id].$item.remove();
    delete items_by_id[id];
}

function place_item ([id, parent, position, url, favicon_url, title, flags]) {
    let item = items_by_id[id];
    item.parent = parent;
    item.position = position;
    let $parent_list = (parent == null ? $toplist : items_by_id[parent].$list);
    for (let $sibling of $parent_list.children) {
        let sibling = items_by_id[+$sibling.id];
        if (position < sibling.position) {
            $parent_list.insertBefore(item.$item, $sibling);
            return;
        }
    }
    $parent_list.append(item.$item);
}

function close_or_delete_tab ($item) {
    if ($item.classList.contains("closed")) {
        host.postMessage(["delete_tab", +$item.id]);
    }
    else {
        host.postMessage(["trash_tab", +$item.id]);
    }
}

function on_tab_clicked (event) {
    let $item = event.target.closest(".item");
    if (event.button == 0) {
        host.postMessage(["focus_tab", +$item.id]);
        items_by_id[+$item.id].$tab.focus();
    }
    else if (event.button == 1) {
        close_or_delete_tab($item);
    }
    handled(event);
}

function on_new_child_clicked (event) {
    let $item = event.target.closest('.item');
    host.postMessage(["new_child", +$item.id]);
    handled(event);
}

function on_close_clicked (event) {
    let $item = event.target.closest('.item');
    close_or_delete_tab($item);
    handled(event);
}

function on_expand_clicked (event) {
    let $item = event.target.closest('.item');
    if ($item.classList.contains("expanded")) {
        host.postMessage(["contract_tab", +$item.id]);
    }
    else {
        host.postMessage(["expand_tab", +$item.id]);
    }
    handled(event);
}

///// Tab dragging and moving

let $tab_destination_marker = $("div", {class:"tab-destination-marker"});

let grabbing_tab = false;
let moving_tab = false;
let moving_tab_id = 0;
let $move_destination = null;
let move_destination_rel = 0;
let tab_move_origin_y = 0;

function on_tab_mousedown (event) {
    let $item = event.target.closest('.item');
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
                host.postMessage([
                    "move_tab", moving_tab_id, +$item.id, move_destination_rel
                ]);
            }
        }
    }
    handled(event);
}

///// Commands from application

let commands = {
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
    },
    view (items) {
        $($toplist, []);
        items_by_id = {};
        for (let item of items) {
            update_item(item);
        }
        for (let item of items) {
            place_item(item);
        }
    },
    update (items) {
        for (let item of items) {
            if (item.length > 1) {
                update_item(item);
            }
            else {
                 // If we're just sent the id, it means remove the tab
                remove_item(item);
            }
        }
        for (let item of items) {
            if (item.length > 1) {
                place_item(item);
            }
        }
    },
    select_location () {
        $address.select();
        $address.focus();
    },
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

///// Misc

function handled (event) {
    event.stopPropagation();
    event.preventDefault();
}

function send_resize () {
    let x = showing_sidebar ? $sidebar.offsetWidth : 0;
    let y = $toolbar.offsetHeight;
    host.postMessage(["resize", x, $toolbar.offsetHeight]);
}

 // Prevent autoscroll behavior
document.addEventListener("mousedown", event => {
    if (event.button == 1) {
        handled(event);
    }
});

///// Create DOM and finish up

$(document.body,
    $toolbar, $sidebar, $main_menu,
);
host.postMessage(["ready"]);
send_resize();

