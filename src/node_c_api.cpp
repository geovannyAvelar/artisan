#include "node_c_api.h"

#include "css.h"
#include "dom_node.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <utility>

namespace {

using artisan::Node;

Node *ToNode(ArtisanNode *node) { return reinterpret_cast<Node *>(node); }

ArtisanNode *FromNode(Node *node) {
  return reinterpret_cast<ArtisanNode *>(node);
}

char *CopyString(const std::string &s) { return strdup(s.c_str()); }

// A copyable handle to a Go closure, for Node::ClickHandler
// (std::function<void()>, which type-erases via a copyable target) - the
// Go-side counterpart to JsCallback in js_engine.cpp. Unlike a JSValue, a
// cgo.Handle has no "dup" operation (Delete()-ing it twice panics), so
// this shares one release across every copy via shared_ptr's refcounting
// instead of manually duplicating/freeing on every copy/destroy the way
// JsCallback does with JS_DupValue/JS_FreeValue. Used for both
// ArtisanNodeSetOnClick and ArtisanNodeAddEventListener below - unlike
// JS (which needs a real {type, target} event object per call, hence
// js_engine.cpp's JsCallback/JsTimerCallback split), every Go handler is
// uniformly zero-arg regardless of what registered it, so one class
// covers both.
class GoCallback {
public:
  explicit GoCallback(uintptr_t handle)
      : handle_(new uintptr_t(handle), [](uintptr_t *h) {
          ArtisanGoReleaseHandler(*h);
          delete h;
        }) {}

  void operator()() const { ArtisanGoInvokeHandler(*handle_); }

private:
  std::shared_ptr<uintptr_t> handle_;
};

// Keeps a freshly created (ArtisanCreateElement/CreateTextNode), not-yet-
// appended Node alive without introducing a second handle *type* across
// the C ABI - every existing function here already takes a plain
// ArtisanNode*, so a created node has to look exactly like any other one.
// AppendChild/InsertBefore below move a pending node's entry out (and
// into the real tree) when it's actually attached; an entry that's never
// attached simply stays here forever - a real, accepted leak (see
// ArtisanCreateElement's doc comment in node_c_api.h), not a bug.
std::unordered_map<Node *, std::unique_ptr<Node>> g_pendingNodes;

ArtisanNode *RegisterPending(std::unique_ptr<Node> node) {
  Node *ptr = node.get();
  g_pendingNodes.emplace(ptr, std::move(node));
  return FromNode(ptr);
}

// Shared by ArtisanNodeAppendChild/InsertBefore: takes `child` out of the
// pending registry and returns it, or nullptr if `child` isn't (or is no
// longer) a pending node - already attached elsewhere, already attached
// here, or just never a created node at all. A real DOM/the JS binding
// both throw for the "already attached" case; there's no exception
// mechanism to report that across a C ABI, so callers here just treat
// nullptr as "do nothing" instead.
std::unique_ptr<Node> TakePending(Node *child) {
  auto it = g_pendingNodes.find(child);
  if (it == g_pendingNodes.end()) {
    return nullptr;
  }
  std::unique_ptr<Node> owned = std::move(it->second);
  g_pendingNodes.erase(it);
  return owned;
}

} // namespace

extern "C" {

ArtisanNode *ArtisanNodeFindById(ArtisanNode *root, const char *id) {
  return FromNode(ToNode(root)->FindById(id));
}

char *ArtisanNodeTagName(ArtisanNode *node) {
  return CopyString(ToNode(node)->tagName());
}

char *ArtisanNodeTextContent(ArtisanNode *node) {
  return CopyString(ToNode(node)->textContent());
}

void ArtisanNodeSetTextContent(ArtisanNode *node, const char *text) {
  ToNode(node)->SetTextContent(text);
}

char *ArtisanNodeGetAttribute(ArtisanNode *node, const char *name) {
  const std::string *value = ToNode(node)->GetAttribute(name);
  return value != nullptr ? CopyString(*value) : nullptr;
}

bool ArtisanNodeHasAttribute(ArtisanNode *node, const char *name) {
  return ToNode(node)->HasAttribute(name);
}

void ArtisanNodeSetAttribute(ArtisanNode *node, const char *name,
                              const char *value) {
  ToNode(node)->SetAttribute(name, value);
}

void ArtisanNodeRemoveAttribute(ArtisanNode *node, const char *name) {
  ToNode(node)->RemoveAttribute(name);
}

ArtisanNode *ArtisanNodeParentNode(ArtisanNode *node) {
  return FromNode(ToNode(node)->parent());
}

ArtisanNode *ArtisanNodeNextSibling(ArtisanNode *node) {
  return FromNode(ToNode(node)->nextSibling());
}

ArtisanNode *ArtisanNodePreviousSibling(ArtisanNode *node) {
  return FromNode(ToNode(node)->previousSibling());
}

size_t ArtisanNodeChildCount(ArtisanNode *node) {
  return ToNode(node)->children().size();
}

ArtisanNode *ArtisanNodeChildAt(ArtisanNode *node, size_t index) {
  return FromNode(ToNode(node)->children()[index].get());
}

ArtisanNode *ArtisanQuerySelector(ArtisanNode *root, const char *selector) {
  return FromNode(artisan::QuerySelector(*ToNode(root), selector));
}

ArtisanNode **ArtisanQuerySelectorAll(ArtisanNode *root, const char *selector,
                                       size_t *outCount) {
  std::vector<Node *> found = artisan::QuerySelectorAll(*ToNode(root), selector);
  *outCount = found.size();
  if (found.empty()) {
    return nullptr;
  }
  auto **array = static_cast<ArtisanNode **>(
      malloc(found.size() * sizeof(ArtisanNode *)));
  for (size_t i = 0; i < found.size(); ++i) {
    array[i] = FromNode(found[i]);
  }
  return array;
}

void ArtisanFreeNodeArray(ArtisanNode **nodes) { free(nodes); }

ArtisanNode *ArtisanCreateElement(const char *tag) {
  return RegisterPending(Node::CreateElement(tag));
}

ArtisanNode *ArtisanCreateTextNode(const char *text) {
  return RegisterPending(Node::CreateText(text));
}

ArtisanNode *ArtisanNodeAppendChild(ArtisanNode *parent, ArtisanNode *child) {
  std::unique_ptr<Node> owned = TakePending(ToNode(child));
  if (!owned) {
    return nullptr;
  }
  return FromNode(ToNode(parent)->AppendChild(std::move(owned)));
}

ArtisanNode *ArtisanNodeInsertBefore(ArtisanNode *parent, ArtisanNode *child,
                                      ArtisanNode *before) {
  std::unique_ptr<Node> owned = TakePending(ToNode(child));
  if (!owned) {
    return nullptr;
  }
  return FromNode(
      ToNode(parent)->InsertBefore(std::move(owned), ToNode(before)));
}

void ArtisanNodeAddEventListener(ArtisanNode *node, const char *eventType,
                                  uintptr_t handle) {
  ToNode(node)->AddEventListener(
      eventType, [cb = GoCallback(handle)](const artisan::Event &) { cb(); });
}

void ArtisanNodeSetOnClick(ArtisanNode *node, uintptr_t handle) {
  ToNode(node)->SetOnClick(GoCallback(handle));
}

void ArtisanFreeString(char *str) { free(str); }

} // extern "C"
