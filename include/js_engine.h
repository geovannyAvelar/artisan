#pragma once

#include "dom_node.h"

#include <memory>
#include <string>

namespace artisan {

// Embeds QuickJS and exposes a minimal, DOM-like binding surface so a
// script - not just hand-written C++ - can read and mutate a Node tree:
//
//   document.getElementById(id)
//   document.createElement(tagName)
//   document.createTextNode(text)
//   node.tagName                          (read-only)
//   node.textContent                      (read/write)
//   node.getAttribute(name)               -> string or null
//   node.setAttribute(name, value)
//   node.appendChild(child)               -> child
//   node.addEventListener("click", fn)
//
// This is the runtime counterpart to artisanc: where artisanc turns
// markup into Node-construction C++ at build time, JsEngine lets an
// actual script build and manipulate the same kind of tree at run time,
// through the same Node API artisanc's generated code and main.cpp both
// use - addEventListener("click", ...) is the same Node::SetOnClick hook
// a C++ caller would use.
//
// Deliberately not exposed: removeChild/remove. This Node model destroys
// a node when it's detached from its parent (see dom_node.h) rather than
// keeping it alive-but-detached the way a real DOM does, so a script
// holding a reference to a since-removed node and then touching it would
// be a use-after-free. Safely supporting removal needs that DOM-style
// detach-not-destroy model first.
class JsEngine {
public:
  // `document` must outlive this JsEngine: scripts run through it, and
  // any addEventListener("click", ...) handlers registered on nodes in
  // it, can only safely run while this object is alive.
  explicit JsEngine(Node &document);
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
