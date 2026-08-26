#pragma once

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
//   node.tagName                          (read-only)
//   node.textContent                      (read/write)
//   node.parentNode / nextSibling / previousSibling / children
//                                          (read-only; snapshots, not a
//                                           live view - see QuerySelector
//                                           in css.h for what that means)
//   node.getAttribute(name)               -> string or null
//   node.setAttribute(name, value)
//   node.hasAttribute(name) / removeAttribute(name)
//   node.appendChild(child) / insertBefore(child, referenceOrNull)
//   node.querySelector(selector) / querySelectorAll(selector)
//   node.addEventListener(type, fn)       -> fn(event), event = {type, target}
//   console.log/warn/error(...)
//   setTimeout(fn, delayMs) / clearTimeout(id)
//   setInterval(fn, delayMs) / clearInterval(id)
//
// This is the runtime counterpart to artisanc: where artisanc turns
// markup into Node-construction C++ at build time, JsEngine lets an
// actual script build and manipulate the same kind of tree at run time,
// through the same Node API artisanc's generated code and main.cpp both
// use - addEventListener is the same Node::AddEventListener/DispatchEvent
// mechanism a C++ or Go caller would ultimately go through too (see
// dom_node.h; SetOnClick/Click() are thin wrappers over it).
//
// Deliberately not exposed: removeChild/remove. This Node model destroys
// a node when it's detached from its parent (see dom_node.h) rather than
// keeping it alive-but-detached the way a real DOM does, so a script
// holding a reference to a since-removed node and then touching it would
// be a use-after-free. Safely supporting removal needs that DOM-style
// detach-not-destroy model first. Also not exposed: removeEventListener -
// a QuickJS JSValue isn't cheaply comparable for identity the way a
// faithful removeEventListener(type, fn) needs, and a token-based
// alternative wouldn't match a real browser's actual signature.
class JsEngine {
public:
  // `document` and `timers` must both outlive this JsEngine: scripts run
  // through `document`, and any addEventListener(...) handlers registered
  // on nodes in it - or any pending setTimeout/setInterval scheduled into
  // `timers` - can only safely run while this object (and its underlying
  // JSContext) is alive. `timers` is not owned here - the caller (the
  // event loop, main.cpp) is expected to call TimerQueue::FireDue itself
  // once per iteration; JsEngine only ever schedules/cancels into it.
  JsEngine(Node &document, TimerQueue &timers);
  ~JsEngine();

  JsEngine(const JsEngine &) = delete;
  JsEngine &operator=(const JsEngine &) = delete;

  // Evaluates `source` as a script. `name` is used only to label
  // exceptions. Returns false (after printing the exception to stderr) if
  // evaluation threw.
  bool RunScript(const std::string &source, const std::string &name);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace artisan
