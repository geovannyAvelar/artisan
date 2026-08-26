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

// Passed to an EventHandler when DispatchEvent fires it - deliberately
// minimal (no bubbling/capturing, no preventDefault): direct dispatch on
// the target only, the same reach Click() already had before it became a
// thin wrapper over this.
struct Event {
  std::string type;
  Node *target;
};

using EventHandler = std::function<void(const Event &)>;

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

  // Detaches `child` from this node and destroys it (and its subtree).
  // Does nothing if `child` isn't actually a child of this node.
  void RemoveChild(Node *child);

  // Detaches this node from its parent (if any) and destroys it. Do not
  // touch `this` after calling it.
  void Remove();

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
  // points those already happen today).
  void AddEventListener(const std::string &type, EventHandler handler);

  // Invokes every handler registered for `type`, in registration order,
  // each with Event{type, this}. No-op if none are registered.
  void DispatchEvent(const std::string &type) const;

private:
  Node(NodeType type, std::string tagName, std::string text);

  NodeType type_;
  std::string tagName_;
  std::string text_;
  std::map<std::string, std::string> attributes_;
  std::map<std::string, std::vector<EventHandler>> listeners_;
  const unsigned char *imageData_ = nullptr;
  int imageDataSize_ = 0;

  Node *parent_ = nullptr;
  std::vector<std::unique_ptr<Node>> children_;
};

} // namespace artisan
