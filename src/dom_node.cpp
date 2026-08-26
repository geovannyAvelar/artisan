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

void Node::RemoveChild(Node *child) {
  auto it = std::find_if(
      children_.begin(), children_.end(),
      [child](const std::unique_ptr<Node> &n) { return n.get() == child; });

  if (it == children_.end()) {
    return;
  }

  (*it)->parent_ = nullptr;
  children_.erase(it); // Destroys `child` (and its subtree).
}

void Node::Remove() {
  if (parent_ != nullptr) {
    parent_->RemoveChild(this);
  }
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
  listeners_["click"].push_back(
      [handler = std::move(handler)](const Event & /*event*/) { handler(); });
}

void Node::Click() const { DispatchEvent("click"); }

void Node::AddEventListener(const std::string &type, EventHandler handler) {
  listeners_[type].push_back(std::move(handler));
}

void Node::DispatchEvent(const std::string &type) const {
  auto it = listeners_.find(type);
  if (it == listeners_.end()) {
    return;
  }
  Event event{type, const_cast<Node *>(this)};
  // Copy the list before iterating: a handler that calls
  // SetOnClick/AddEventListener/RemoveAttribute etc. on this same node
  // could otherwise invalidate listeners_'s iterators mid-dispatch.
  std::vector<EventHandler> handlers = it->second;
  for (const EventHandler &handler : handlers) {
    handler(event);
  }
}

} // namespace artisan
