#include "dom_node.h"

#include <algorithm>
#include <iterator>

namespace artisan {

Node::Node(NodeType type, std::string tagName, std::string text)
    : type_(type), tagName_(std::move(tagName)), text_(std::move(text)) {}

std::unique_ptr<Node> Node::CreateElement(const std::string &tagName) {
  return std::unique_ptr<Node>(new Node(NodeType::kElement, tagName, ""));
}

std::unique_ptr<Node> Node::CreateText(const std::string &text) {
  return std::unique_ptr<Node>(new Node(NodeType::kText, "", text));
}

std::string Node::textContent() const {
  if (type_ == NodeType::kText) {
    return text_;
  }

  std::string result;
  for (const auto &child : children_) {
    result += child->textContent();
  }
  return result;
}

void Node::SetTextContent(const std::string &text) {
  if (type_ == NodeType::kText) {
    text_ = text;
    return;
  }

  children_.clear();
  AppendChild(CreateText(text));
}

const std::string *Node::GetAttribute(const std::string &name) const {
  auto it = attributes_.find(name);
  return it == attributes_.end() ? nullptr : &it->second;
}

void Node::SetAttribute(const std::string &name, const std::string &value) {
  attributes_[name] = value;
}

void Node::RemoveAttribute(const std::string &name) { attributes_.erase(name); }

void Node::SetImageData(const unsigned char *data, int size) {
  imageData_ = data;
  imageDataSize_ = size;
}

std::unique_ptr<Node> Node::CloneNode(bool deep) const {
  std::unique_ptr<Node> clone = type_ == NodeType::kElement
                                     ? CreateElement(tagName_)
                                     : CreateText(text_);
  clone->attributes_ = attributes_;
  // imageData_ is deliberately not copied - it's either a pointer into
  // artisanc-embedded .rodata (fine to alias) or something the app set
  // programmatically and may not want the clone silently sharing;
  // real DOM's cloneNode doesn't have a direct analog here either way,
  // so this stays unset on the clone like any other newly created node.

  if (deep) {
    for (const std::unique_ptr<Node> &child : children_) {
      clone->AppendChild(child->CloneNode(true));
    }
  }

  return clone;
}

Node *Node::AppendChild(std::unique_ptr<Node> child) {
  child->parent_ = this;
  Node *ptr = child.get();
  children_.push_back(std::move(child));
  return ptr;
}

Node *Node::InsertBefore(std::unique_ptr<Node> child, Node *before) {
  auto it =
      before == nullptr
          ? children_.end()
          : std::find_if(children_.begin(), children_.end(),
                          [before](const std::unique_ptr<Node> &n) {
                            return n.get() == before;
                          });

  child->parent_ = this;
  Node *ptr = child.get();
  children_.insert(it, std::move(child));
  return ptr;
}

Node *Node::nextSibling() const {
  if (parent_ == nullptr) {
    return nullptr;
  }
  const auto &siblings = parent_->children_;
  auto it = std::find_if(
      siblings.begin(), siblings.end(),
      [this](const std::unique_ptr<Node> &n) { return n.get() == this; });
  if (it == siblings.end() || std::next(it) == siblings.end()) {
    return nullptr;
  }
  return std::next(it)->get();
}

Node *Node::previousSibling() const {
  if (parent_ == nullptr) {
    return nullptr;
  }
  const auto &siblings = parent_->children_;
  auto it = std::find_if(
      siblings.begin(), siblings.end(),
      [this](const std::unique_ptr<Node> &n) { return n.get() == this; });
  if (it == siblings.end() || it == siblings.begin()) {
    return nullptr;
  }
  return std::prev(it)->get();
}

std::unique_ptr<Node> Node::RemoveChild(Node *child) {
  auto it = std::find_if(
      children_.begin(), children_.end(),
      [child](const std::unique_ptr<Node> &n) { return n.get() == child; });

  if (it == children_.end()) {
    return nullptr;
  }

  std::unique_ptr<Node> owned = std::move(*it);
  children_.erase(it);
  owned->parent_ = nullptr;
  return owned;
}

std::unique_ptr<Node> Node::Remove() {
  if (parent_ == nullptr) {
    return nullptr;
  }
  return parent_->RemoveChild(this);
}

Node *Node::FindById(const std::string &id) {
  if (type_ == NodeType::kElement) {
    const std::string *nodeId = GetAttribute("id");
    if (nodeId != nullptr && *nodeId == id) {
      return this;
    }
  }

  for (auto &child : children_) {
    Node *found = child->FindById(id);
    if (found != nullptr) {
      return found;
    }
  }

  return nullptr;
}

void Node::SetOnClick(ClickHandler handler) {
  // Replace, not append - matches `element.onclick = fn` (as opposed to
  // AddEventListener, which - like real addEventListener - never
  // replaces anything already registered).
  listeners_["click"].clear();
  AddEventListener(
      "click", [handler = std::move(handler)](Event & /*event*/) { handler(); });
}

void Node::Click() const { DispatchEvent("click"); }

void Node::AddEventListener(const std::string &type, EventHandler handler,
                             bool capture) {
  listeners_[type].push_back(Listener{std::move(handler), capture});
}

int Node::RemoveEventListener(
    const std::string &type, bool capture,
    const std::function<bool(const EventHandler &)> &predicate) {
  auto it = listeners_.find(type);
  if (it == listeners_.end()) {
    return 0;
  }
  std::vector<Listener> &entries = it->second;
  size_t before = entries.size();
  entries.erase(std::remove_if(entries.begin(), entries.end(),
                                [&](const Listener &l) {
                                  return l.capture == capture &&
                                         predicate(l.handler);
                                }),
                entries.end());
  return static_cast<int>(before - entries.size());
}

void Node::DispatchAt(const std::string &type, Event &event,
                       bool includeCapture, bool includeBubble) const {
  auto it = listeners_.find(type);
  if (it == listeners_.end()) {
    return;
  }
  // Copy the list before iterating: a handler that calls
  // AddEventListener/RemoveEventListener/RemoveAttribute etc. on this
  // same node could otherwise invalidate listeners_'s iterators
  // mid-dispatch.
  std::vector<Listener> entries = it->second;
  for (const Listener &entry : entries) {
    if (event.ImmediatePropagationStopped()) {
      return;
    }
    if ((entry.capture && includeCapture) ||
        (!entry.capture && includeBubble)) {
      entry.handler(event);
    }
  }
}

bool Node::DispatchEvent(const std::string &type, bool bubbles,
                          bool cancelable, const void *detail) const {
  Event event(type, const_cast<Node *>(this), bubbles, cancelable);
  event.detail = detail;

  if (bubbles) {
    // Capturing phase: root -> this node's immediate parent, in that
    // order (outermost ancestor first) - collect ancestors first since
    // the walk needs to go root-to-target, the opposite direction
    // parent_ chases.
    std::vector<Node *> ancestors;
    for (Node *p = parent_; p != nullptr; p = p->parent_) {
      ancestors.push_back(p);
    }
    for (auto it = ancestors.rbegin(); it != ancestors.rend(); ++it) {
      (*it)->DispatchAt(type, event, /*includeCapture=*/true,
                         /*includeBubble=*/false);
      if (event.PropagationStopped()) {
        return event.DefaultPrevented();
      }
    }
  }

  // Target phase: both capture- and non-capture-registered listeners on
  // this node fire here, in registration order - the two phases
  // converge at the target itself, and it always runs regardless of
  // `bubbles` (that flag only affects whether the ancestor phases do).
  DispatchAt(type, event, /*includeCapture=*/true, /*includeBubble=*/true);
  if (event.PropagationStopped()) {
    return event.DefaultPrevented();
  }

  if (bubbles) {
    // Bubbling phase: this node's parent -> root.
    for (Node *p = parent_; p != nullptr; p = p->parent_) {
      p->DispatchAt(type, event, /*includeCapture=*/false,
                    /*includeBubble=*/true);
      if (event.PropagationStopped()) {
        return event.DefaultPrevented();
      }
    }
  }

  return event.DefaultPrevented();
}

} // namespace artisan
