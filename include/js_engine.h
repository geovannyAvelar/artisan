#pragma once

#include "animation_frame_queue.h"
#include "dom_node.h"
#include "timer_queue.h"

#include <memory>
#include <string>

namespace artisan {

// Embeds QuickJS and exposes a DOM-like binding surface so a script -
// not just hand-written C++ - can read and mutate a Node tree:
//
//   document.getElementById(id)
//   document.querySelector(selector) / querySelectorAll(selector)
//   document.createElement(tagName)
//   document.createTextNode(text)
//   document.addEventListener(type, fn, captureOrOptions) /
//   document.removeEventListener(type, fn, captureOrOptions)
//                                           (same semantics as
//                                            node.addEventListener below,
//                                            registered on the root - for
//                                            event delegation: one
//                                            listener here catches every
//                                            bubbled event from anywhere
//                                            in the tree)
//   node.tagName / nodeType                (read-only; nodeType is 1 for
//                                            an element, 3 for text - see
//                                            also the ELEMENT_NODE/
//                                            TEXT_NODE constants below)
//   node.textContent                       (read/write)
//   node.nodeValue                         (read/write; alias for
//                                            textContent above - real
//                                            DOM only defines this on a
//                                            Text node, but this model
//                                            doesn't draw that
//                                            distinction. Exists
//                                            because a real DOM
//                                            reconciler - react-dom's
//                                            included - updates an
//                                            existing text node by
//                                            setting this property
//                                            directly, not a method
//                                            call)
//   node.parentNode / nextSibling / previousSibling
//                                           (read-only; re-read from the
//                                            tree on every access, so
//                                            already "live" in the only
//                                            sense that means anything
//                                            for a single node reference)
//   node.ownerDocument                     (read-only; the one global
//                                            `document` object, `===` on
//                                            every node regardless of
//                                            tree position or attachment
//                                            - there's only ever one
//                                            document here, so unlike a
//                                            real multi-document DOM
//                                            there's no "created by a
//                                            different document" case to
//                                            tell apart)
//   node.children                          (read-only; a live
//                                            HTMLCollection-like object -
//                                            .length and node.children[i]
//                                            both re-read the tree on
//                                            every access, so an
//                                            append/remove after the
//                                            fact is visible through a
//                                            `var kids = node.children`
//                                            held from before it - see
//                                            "--- children ---" in
//                                            js_engine.cpp. Not iterable
//                                            (no for-of/spread/forEach -
//                                            matches real HTMLCollection,
//                                            historically; Object.keys/
//                                            for-in/Array.from all work)
//                                            and read-only (assigning an
//                                            index is a silent no-op).
//                                            querySelector(All) below
//                                            stay real snapshot arrays,
//                                            matching real DOM (unlike
//                                            children/childNodes,
//                                            querySelectorAll is
//                                            specified as static, not
//                                            live)
//   node.getAttribute(name)                -> string or null
//   node.setAttribute(name, value)
//   node.hasAttribute(name) / removeAttribute(name)
//   node.classList.add/remove/toggle/contains(...)
//   node.style.color/backgroundColor/fontWeight/borderColor/borderWidth/
//        width/height/paddingTop/paddingRight/paddingBottom/paddingLeft/
//        marginTop/marginRight/marginBottom/marginLeft/display/
//        flexDirection/justifyContent/alignItems/alignContent/gap/
//        flexWrap/flexGrow/flexShrink/flexBasis/cssText
//                                           (read/write; every property
//                                            css.h's Declarations
//                                            understands, camelCase -
//                                            see css.h/ParseDeclarations)
//   node.dataset.fooBar                    (read/write/delete; real
//                                            property syntax over the
//                                            node's data-* attributes -
//                                            "fooBar" <-> "data-foo-bar",
//                                            same mapping getData/setData
//                                            below use. `delete
//                                            node.dataset.foo` removes
//                                            the attribute.
//                                            Object.keys/for-in/
//                                            JSON.stringify all see
//                                            exactly the node's data-*
//                                            keys, camelCased - not
//                                            iterable (no for-of/spread),
//                                            same simplification as
//                                            node.children below)
//   node.getData(name) / setData(name, value)
//                                           (an older, method-based way
//                                            to reach the exact same
//                                            data-* attributes as
//                                            node.dataset above - kept
//                                            for existing callers, not
//                                            because dataset can't do it)
//   node.appendChild(child) / insertBefore(child, referenceOrNull)
//   node.removeChild(child) / remove()     -> the removed node, still
//                                            alive and re-appendable
//   node.cloneNode(deep)
//   node.matches(selector) / closest(selector)
//   node.querySelector(selector) / querySelectorAll(selector)
//   node.addEventListener(type, fn, captureOrOptions)
//                                           -> fn(event); event has type/
//                                            target/bubbles/cancelable/
//                                            detail/preventDefault()/
//                                            stopPropagation()/
//                                            stopImmediatePropagation()/
//                                            defaultPrevented, plus
//                                            MouseEvent/KeyboardEvent
//                                            data (clientX/clientY/
//                                            ctrlKey/shiftKey/altKey/
//                                            metaKey/key/code - 0/false/
//                                            "" on an event that isn't
//                                            one of those) - populated
//                                            on "click" (real click
//                                            position/modifiers) and
//                                            "keydown" (fired whenever a
//                                            focused input gets a key
//                                            press - calling
//                                            preventDefault() on it
//                                            suppresses main.cpp's own
//                                            built-in editing for that
//                                            key: character insertion,
//                                            backspace/delete, arrow/
//                                            home/end cursor movement,
//                                            and the ctrl+a/c/x/v
//                                            shortcuts alike, same as a
//                                            real browser suppressing
//                                            its default keydown
//                                            handling on an <input>)
//   node.removeEventListener(type, fn, captureOrOptions)
//   node.dispatchEvent(event) -> !defaultPrevented; walks the same
//                                            capturing/target/bubbling
//                                            phases as an internally
//                                            fired click/change/input
//   new Event(type, {bubbles, cancelable}) /
//   new CustomEvent(type, {detail, bubbles, cancelable})
//                                           -> an event object usable
//                                            with dispatchEvent above (or
//                                            fired standalone -
//                                            preventDefault()/etc. on one
//                                            that's never dispatched are
//                                            harmless no-ops)
//   Node.ELEMENT_NODE / Node.TEXT_NODE
//   console.log/warn/error(...)
//   setTimeout(fn, delayMs) / clearTimeout(id)
//   setInterval(fn, delayMs) / clearInterval(id)
//   requestAnimationFrame(fn) -> fn(timestampMs) / cancelAnimationFrame(id)
//   h(tag, props, ...children) / Fragment(props)
//                                           -> the framework-agnostic
//                                            JSX target every .jsx file
//                                            compiles against (see
//                                            tools/jsx_transform) - a
//                                            plain element-builder over
//                                            the same Node API above,
//                                            not a virtual DOM. `tag` a
//                                            string builds a real
//                                            element (props: "on<type>"
//                                            - lowercase, e.g. "onclick"
//                                            - adds a listener,
//                                            everything else sets a
//                                            same-named attribute;
//                                            children: an array
//                                            recurses, string/number
//                                            becomes a text node,
//                                            null/undefined/boolean is
//                                            skipped). `tag` a function
//                                            (a component, or Fragment
//                                            itself) is called with one
//                                            `props` object (children
//                                            merged in as
//                                            `props.children`) and its
//                                            return value passed
//                                            through as-is. Plain,
//                                            ordinary globals - a
//                                            project wanting a real UI
//                                            library (React or
//                                            otherwise) instead just
//                                            reassigns them itself, see
//                                            README.md's "Using
//                                            JavaScript" section.
//
// This is the runtime counterpart to artisanc: where artisanc turns
// markup into Node-construction C++ at build time, JsEngine lets an
// actual script build and manipulate the same kind of tree at run time,
// through the same Node API artisanc's generated code and main.cpp both
// use - addEventListener is the same Node::AddEventListener/DispatchEvent
// mechanism a C++ or Go caller would ultimately go through too (see
// dom_node.h; SetOnClick/Click() are thin wrappers over it).
//
// A few real gaps, not just missing bindings: classList/style cover
// exactly what the rest of this Node model already covers (the "class"
// attribute's tokens; the same five properties a <style> block
// supports) - not real CSS's full surface. And a second,
// independently-obtained reference to a node removed (see removeChild/
// remove above) through a *different* reference can dangle if the
// reference that now owns it gets garbage-collected first.
//
// Node identity: WrapExistingNode/WrapOwnedNode (js_engine.cpp) keep a
// weak Node* -> JSValue cache, so two references to the same underlying
// node - document.getElementById(id) called twice, a dispatched event's
// `.target` vs. a script's own saved reference, a node handed back by
// createElement and then re-found via querySelector - come back as the
// *same* JS object, and `===` between them holds. What that cache
// doesn't do: the *Event* object a listener receives is still a fresh
// object per listener, per dispatchEvent(event) call (carrying the same
// type/detail/bubbles/cancelable/target - not `event` itself), even
// though its `.target` property is now the identity-cached node, so
// `event.target === savedRef` holds even where `event === event` across
// two listeners of the same dispatch would not.
class JsEngine {
public:
  // `document`, `timers`, and `animationFrames` must all outlive this
  // JsEngine: scripts run through `document`, and any addEventListener/
  // setTimeout/setInterval/requestAnimationFrame registrations can only
  // safely fire while this object (and its underlying JSContext) is
  // alive. Neither queue is owned here - the caller (the event loop,
  // main.cpp) is expected to call TimerQueue::FireDue/
  // AnimationFrameQueue::FireAll itself once per iteration; JsEngine only
  // ever schedules/cancels into them.
  //
  // Concretely, that means `document` must be *destroyed before* this
  // JsEngine, not after (main.cpp's own navigate() already does exactly
  // this: document.reset() first, jsEngine.reset() second) - counter-
  // intuitive if "outlive" is read as a plain stack-declaration-order
  // rule, where declaring `document` first would destroy it *after* a
  // later-declared JsEngine. The reason: node.addEventListener(...)
  // stores its JS callback inside the Node itself (Node::listeners_,
  // entirely outside JsEngine's own ownership), holding a real JS
  // reference (JsCallback) that has to be freed - via that Node's own
  // destructor - while this JsEngine's JSContext is still alive to free
  // it into. Destroying this JsEngine first leaves that reference
  // dangling: harmless as long as `document` never dispatches again
  // afterward, but it trips QuickJS's own internal consistency assertion
  // in JS_FreeRuntime (list_empty(&rt->gc_obj_list)) on process exit -
  // real, reproducible, and specifically what first surfaced this
  // ordering requirement.
  JsEngine(Node &document, TimerQueue &timers,
           AnimationFrameQueue &animationFrames);
  ~JsEngine();

  JsEngine(const JsEngine &) = delete;
  JsEngine &operator=(const JsEngine &) = delete;

  // Evaluates `source` as a script. `name` is used only to label
  // exceptions. Returns false (after printing the exception to stderr) if
  // evaluation threw.
  bool RunScript(const std::string &source, const std::string &name);

  // Drains QuickJS's job queue - every pending Promise .then()/.catch()/
  // async-function continuation - running each to completion (printing
  // and swallowing an exception the same way every other script entry
  // point here does, rather than letting one throw take down the rest of
  // the queue). Without this, a script's Promises/async functions would
  // parse and start running fine but their continuations would just
  // never fire - QuickJS doesn't drive its own job queue, an embedder
  // has to (see JS_ExecutePendingJob in quickjs.h). Expected to be
  // called once per event-loop iteration (main.cpp), the same footing
  // as TimerQueue::FireDue/AnimationFrameQueue::FireAll - not a per-
  // macrotask-precise drain (real engines flush microtasks after *every*
  // callback, not once per loop iteration), but enough for a
  // continuation to actually run, within the same iteration or the next
  // one rather than never. Returns whether anything actually ran, same
  // reason FireDue/FireAll do - a continuation mutating the DOM should
  // trigger a redraw.
  bool PumpJobQueue();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace artisan
