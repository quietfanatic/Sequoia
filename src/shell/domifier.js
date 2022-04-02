"use strict";

// This is a shortcut function for creating DOM elements in script.

// If element is a string, creates a new element with that tag name.
// If element is an Element, modifies that element inplace.
// The other arguments are interpreted as follows:
//   - Any arguments that are arrays will be interpreted as more arguments.
//   - For any arguments that are objects, for each key:value pair:
//     - If the value is a function, it will be added as an event listener on
//       the element.
//     - If the value is an object, it can contain both options to
//       addEventListener (passive, capture, etc.) and a handleEvent method
//       which is the handler.
//     - Anything else will be interpreted as a string an set as an attribute
//       on the element.
//   - If there are any non-object arguments, the element's content will be
//     cleared and replaced with the arguments, which can be Nodes, stringish
//     things, or arrays of Nodes and stringish things.  To clear the content
//     without replacing it with anything, use an empty array.  You cannot use
//     this API to remove attributes or event listeners.
// If you want to do anything else, just use the regular DOM API.

function $ (element, ...mods) {
    let $element;
    if (element instanceof Element) {
        $element = element;
    }
    else {
        $element = document.createElement(element);
    }
    let clear_once = () => {
        while ($element.firstChild) {
            $element.removeChild($element.firstChild);
        }
        clear_once = () => {};
    };
    apply_mods(mods);
    function apply_mods (mods) {
        for (let mod of mods) {
            if (mod instanceof Node) {
                clear_once();
                $element.appendChild(mod);
            }
            else if (Array.isArray(mod)) {
                clear_once();
                apply_mods(mod);
            }
            else if (typeof mod === "object") {
                for (let name in mod) {
                    if (typeof mod[name] === "function") {
                        $element.addEventListener(name, mod[name]);
                    }
                    else if (typeof mod[name] === "object") {
                        $element.addEventListener(name, mod[name], mod[name]);
                    }
                    else {
                        $element.setAttribute(name, mod[name]);
                    }
                }
            }
            else {
                clear_once();
                $element.appendChild(document.createTextNode(mod));
            }
        }
    }
    return $element;
}

