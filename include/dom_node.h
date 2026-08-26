#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace artisan {

enum class NodeType {
  kElement,
  kText,
};

class Node;

// Passed to an EventHandler when DispatchEvent fires it. Mutable (not a
// plain data struct): a handler calls PreventDefault/StopPropagation/
// StopImmediatePropagation on it, and DispatchEvent's own capturing/
// bubbling walk (see dom_node.cpp) checks those flags between each
// ancestor/listener to decide whether to keep going - the same shape
// real DOM's Event has, just without the bubbles/cancelable/composed
// configuration flags real DOM lets a dispatcher set (every event this
// Node model fires bubbles and is cancelable, unconditionally).
class Event {
public:
  Event(std::string type, Node *target) : type(std::move(type)), target(target) {}

  std::string type;
  Node *target;

  void PreventDefault() { defaultPrevented_ = true; }
  bool DefaultPrevented() const { return defaultPrevented_; }

  void StopPropagation() { propagationStopped_ = true; }
  bool PropagationStopped() const { return propagationStopped_; }

  // Also implies StopPropagation - stopping "immediately" means neither
  // any remaining listener at the current node nor any further
  // ancestor gets a turn, same as real DOM.
  void StopImmediatePropagation() {
    propagationStopped_ = true;
    immediatePropagationStopped_ = true;
  }
  bool ImmediatePropagationStopped() const {
    return immediatePropagationStopped_;
  }

private:
  bool defaultPrevented_ = false;
  bool propagationStopped_ = false;
  bool immediatePropagationStopped_ = false;
};

using EventHandler = std::function<void(Event &)>;

// A single node of the mutable DOM tree a future scripting layer will
// manipulate at runtime - create/append/remove children, set attributes,
// change text. Unlike Widget (the compiled tree artisanc bakes into the
// binary as read-only data), a Node is real heap-allocated, mutable
// state: this is the side of the architecture that supports runtime
// interactivity, not build-time compilation.
//
// Ownership: a node owns its children (via unique_ptr in `children_`);
// `parent()` is a non-owning back-pointer - ownership only ever flows
// parent -> child, so a child can never outlive the parent holding it,
// and there's no cycle for a raw back-pointer to create a problem with.
class Node {
public:
  static std::unique_ptr<Node> CreateElement(const std::string &tagName);
  static std::unique_ptr<Node> CreateText(const std::string &text);

  Node(const Node &) = delete;
  Node &operator=(const Node &) = delete;
  Node(Node &&) = delete;
  Node &operator=(Node &&) = delete;

  NodeType type() const { return type_; }

  // kElement only ("" for a text node).
  const std::string &tagName() const { return tagName_; }

  // kText: this node's own text. kElement: the concatenation of the
  // text-node content of every descendant, depth-first.
  std::string textContent() const;

  // On a text node, replaces its text. On an element, matches real DOM
  // `.textContent =` semantics: discards every existing child and
  // replaces them with a single new text node.
  void SetTextContent(const std::string &text);

  // kElement only. Returns nullptr if the attribute isn't set.
  const std::string *GetAttribute(const std::string &name) const;
  bool HasAttribute(const std::string &name) const {
    return GetAttribute(name) != nullptr;
  }
  void SetAttribute(const std::string &name, const std::string &value);
  void RemoveAttribute(const std::string &name);

  // kElement only: raw encoded (PNG/JPEG/GIF/WebP) image bytes this node
  // displays - set by artisanc for an <img> compiled from markup (the
  // file's bytes embedded into the binary's .rodata at build time, so no
  // runtime file I/O), or programmatically. `data` must remain valid for
  // this Node's lifetime; it is not copied or owned here. nullptr/0 if
  // unset.
  void SetImageData(const unsigned char *data, int size);
  const unsigned char *imageData() const { return imageData_; }
  int imageDataSize() const { return imageDataSize_; }

  // A new, detached copy of this node - same tag/text/attributes, never
  // event listeners (matching real DOM's cloneNode, which never copies
  // them either). `deep`: also recursively clones every descendant;
  // false clones just this one node (an element's children are simply
  // absent from the copy, not an empty placeholder for them).
  std::unique_ptr<Node> CloneNode(bool deep) const;

  Node *parent() const { return parent_; }
  const std::vector<std::unique_ptr<Node>> &children() const {
    return children_;
  }

  // nullptr if this is the last/first child, or has no parent at all
  // (found by identity in parent()->children(), same as a real DOM would
  // track via prev/next pointers - this tree just doesn't keep those, so
  // it's a linear scan instead).
  Node *nextSibling() const;
  Node *previousSibling() const;

  // Depth-first search of this subtree (including this node itself) for
  // an element whose "id" attribute equals `id`. nullptr if none match -
  // the lookup a future scripting layer's getElementById would use, and
  // how compiled markup's ids get wired to handlers today (see main.cpp).
  Node *FindById(const std::string &id);

  // Takes ownership of `child`, appending it after any existing children.
  // Returns a non-owning pointer to it.
  Node *AppendChild(std::unique_ptr<Node> child);

  // Takes ownership of `child`, inserting it directly before `before` (a
  // no-op search failure - `before` not actually a child of this node -
  // falls back to appending, same as AppendChild). `before == nullptr`
  // means append at the end, matching real DOM insertBefore(node, null).
  // Returns a non-owning pointer to `child`.
  Node *InsertBefore(std::unique_ptr<Node> child, Node *before);

  // Detaches `child` and returns ownership of it (still fully alive,
  // just no longer part of this tree - matching real DOM's removeChild,
  // which also returns the removed node) - nullptr if `child` isn't
  // actually a child of this node. A caller that discards the return
  // value gets today's "destroy on removal" behavior for free (the
  // unique_ptr just goes out of scope); one that keeps it can inspect it
  // or re-append it elsewhere via AppendChild/InsertBefore.
  std::unique_ptr<Node> RemoveChild(Node *child);

  // Detaches this node from its parent (if any) and returns ownership of
  // it (nullptr if it had no parent) - see RemoveChild above for what
  // "returns ownership" means for a caller that doesn't care.
  std::unique_ptr<Node> Remove();

  using ClickHandler = std::function<void()>;

  // kElement only. Registers *the* click handler, replacing any previous
  // one - same as a real DOM's `element.onclick = fn` (as opposed to
  // AddEventListener("click", ...) below, which - like real
  // addEventListener - accumulates independently of this and of each
  // other). Implemented as a thin wrapper over AddEventListener; kept
  // around (rather than folded away) since every existing caller - C++
  // apps, the Go bridge (node_c_api.cpp) - already depends on this exact
  // zero-argument signature.
  void SetOnClick(ClickHandler handler);

  // Invokes every registered click handler (SetOnClick's, plus any
  // AddEventListener("click", ...) ones) - a thin wrapper over
  // DispatchEvent("click"). No-op if none are registered.
  void Click() const;

  // kElement only. Registers `handler` for `type`, alongside (not
  // replacing) any other handler already registered for that type -
  // "type" is an open string, not an enum: this Node model doesn't
  // predefine which event types exist, any more than a real DOM's
  // addEventListener does. main.cpp is what actually decides when to
  // fire which types (DispatchEvent("click")/"change"/"input" at the
  // points those already happen today). `capture`: fires during the
  // capturing phase (root -> target) instead of the bubbling phase
  // (target -> root) - see DispatchEvent below for what that means.
  void AddEventListener(const std::string &type, EventHandler handler,
                         bool capture = false);

  // Removes every listener registered for `type`/`capture` for which
  // `predicate(handler)` returns true. Node itself has no notion of "the
  // same function" - EventHandler is a type-erased std::function, not
  // comparable - so that comparison is entirely the caller's job; the JS
  // binding's predicate uses std::function::target<JsCallback>() to
  // recover the original JS function and compares its identity (see
  // js_engine.cpp). Returns how many were removed.
  int RemoveEventListener(const std::string &type, bool capture,
                           const std::function<bool(const EventHandler &)> &predicate);

  // Dispatches `type` at this node with the standard three-phase walk:
  // capturing (root -> this node's parent, capture-registered listeners
  // only), target (both capture- and non-capture-registered listeners on
  // this node itself, in registration order), then bubbling (this node's
  // parent -> root, non-capture listeners only) - same shape real DOM's
  // dispatchEvent uses, just without a way to opt an individual dispatch
  // out of bubbling (every event fired here bubbles). A handler calling
  // event.StopPropagation()/StopImmediatePropagation() halts the walk
  // early, checked between each ancestor/listener. Returns whether any
  // handler called PreventDefault() - main.cpp uses this to decide
  // whether e.g. a <a href> click should actually navigate.
  bool DispatchEvent(const std::string &type) const;

private:
  Node(NodeType type, std::string tagName, std::string text);

  struct Listener {
    EventHandler handler;
    bool capture;
  };

  // Invokes listeners_[type] (a defensive copy first - a handler that
  // adds/removes listeners on this same node mid-dispatch shouldn't
  // invalidate the vector being iterated), stopping early if `event`'s
  // immediate-propagation flag gets set. `includeCapture`/`includeBubble`
  // select which of this node's own capture-flagged/non-capture-flagged
  // listeners actually run - DispatchEvent calls this with only one of
  // the two true for an ancestor (capturing or bubbling phase) and both
  // true for the target node itself (both phases converge there).
  void DispatchAt(const std::string &type, Event &event, bool includeCapture,
                   bool includeBubble) const;

  NodeType type_;
  std::string tagName_;
  std::string text_;
  std::map<std::string, std::string> attributes_;
  std::map<std::string, std::vector<Listener>> listeners_;
  const unsigned char *imageData_ = nullptr;
  int imageDataSize_ = 0;

  Node *parent_ = nullptr;
  std::vector<std::unique_ptr<Node>> children_;
};

} // namespace artisan
