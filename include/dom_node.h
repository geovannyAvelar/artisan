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

  // Depth-first search of this subtree (including this node itself) for
  // an element whose "id" attribute equals `id`. nullptr if none match -
  // the lookup a future scripting layer's getElementById would use, and
  // how compiled markup's ids get wired to handlers today (see main.cpp).
  Node *FindById(const std::string &id);

  // Takes ownership of `child`, appending it after any existing children.
  // Returns a non-owning pointer to it.
  Node *AppendChild(std::unique_ptr<Node> child);

  // Detaches `child` from this node and destroys it (and its subtree).
  // Does nothing if `child` isn't actually a child of this node.
  void RemoveChild(Node *child);

  // Detaches this node from its parent (if any) and destroys it. Do not
  // touch `this` after calling it.
  void Remove();

  using ClickHandler = std::function<void()>;

  // kElement only. Registers the handler Click() invokes - the hook a
  // future scripting layer's addEventListener would bind to. Typically
  // used for <button>; harmless on other elements today since nothing
  // else in this tree calls Click() on them.
  void SetOnClick(ClickHandler handler);

  // Invokes the click handler if one is set. No-op otherwise.
  void Click() const;

private:
  Node(NodeType type, std::string tagName, std::string text);

  NodeType type_;
  std::string tagName_;
  std::string text_;
  std::map<std::string, std::string> attributes_;
  ClickHandler onClick_;
  const unsigned char *imageData_ = nullptr;
  int imageDataSize_ = 0;

  Node *parent_ = nullptr;
  std::vector<std::unique_ptr<Node>> children_;
};

} // namespace artisan
