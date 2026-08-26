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
//   node.tagName / nodeType                (read-only; nodeType is 1 for
//                                            an element, 3 for text - see
//                                            also the ELEMENT_NODE/
//                                            TEXT_NODE constants below)
//   node.textContent                       (read/write)
//   node.parentNode / nextSibling / previousSibling / children
//                                           (read-only; snapshots, not a
//                                            live view - see QuerySelector
//                                            in css.h for what that means)
//   node.getAttribute(name)                -> string or null
//   node.setAttribute(name, value)
//   node.hasAttribute(name) / removeAttribute(name)
//   node.classList.add/remove/toggle/contains(...)
//   node.style.color/backgroundColor/fontWeight/borderColor/borderWidth
//                                           (read/write; exactly the five
//                                            properties a <style> block
//                                            supports - see css.h)
//   node.getData(name) / setData(name, value)
//                                           (dataset's data-* attributes,
//                                            but method-based rather than
//                                            true dataset.foo property
//                                            syntax - see js_engine.cpp)
//   node.appendChild(child) / insertBefore(child, referenceOrNull)
//   node.removeChild(child) / remove()     -> the removed node, still
//                                            alive and re-appendable
//   node.cloneNode(deep)
//   node.matches(selector) / closest(selector)
//   node.querySelector(selector) / querySelectorAll(selector)
//   node.addEventListener(type, fn, captureOrOptions)
//                                           -> fn(event); event has type/
//                                            target/preventDefault()/
//                                            stopPropagation()/
//                                            stopImmediatePropagation()/
//                                            defaultPrevented
//   node.removeEventListener(type, fn, captureOrOptions)
//   Node.ELEMENT_NODE / Node.TEXT_NODE
//   console.log/warn/error(...)
//   setTimeout(fn, delayMs) / clearTimeout(id)
//   setInterval(fn, delayMs) / clearInterval(id)
//   requestAnimationFrame(fn) -> fn(timestampMs) / cancelAnimationFrame(id)
//
// This is the runtime counterpart to artisanc: where artisanc turns
// markup into Node-construction C++ at build time, JsEngine lets an
// actual script build and manipulate the same kind of tree at run time,
// through the same Node API artisanc's generated code and main.cpp both
// use - addEventListener is the same Node::AddEventListener/DispatchEvent
// mechanism a C++ or Go caller would ultimately go through too (see
// dom_node.h; SetOnClick/Click() are thin wrappers over it).
//
// A few real gaps, not just missing bindings: node.classList/style/
// getData/setData are JS-only for now (Go's node_c_api.h doesn't have
// them yet - a natural follow-up, not attempted here). classList/style
// cover exactly what the rest of this Node model already covers (the
// "class" attribute's tokens; the same five properties a <style> block
// supports) - not real CSS's full surface. There's no CustomEvent/
// script-side dispatchEvent(arbitraryEvent) - only the event types
// main.cpp already fires internally (click/change/input) go through the
// real bubbling/capturing/preventDefault machinery; a script can't
// construct and fire its own event. And every wrapped node is a fresh JS
// object per call (WrapExistingNode, js_engine.cpp) - `===` between two
// references to the same underlying node is always false, and a second,
// independently-obtained reference to a node removed (see removeChild/
// remove above) through a *different* reference can dangle if the
// reference that now owns it gets garbage-collected first.
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
  JsEngine(Node &document, TimerQueue &timers,
           AnimationFrameQueue &animationFrames);
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
