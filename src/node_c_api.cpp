#include "node_c_api.h"

#include "dom_node.h"

#include <cstdlib>
#include <cstring>
#include <memory>
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
// JsCallback does with JS_DupValue/JS_FreeValue.
class GoCallback {
public:
  explicit GoCallback(uintptr_t handle)
      : handle_(new uintptr_t(handle), [](uintptr_t *h) {
          ArtisanGoReleaseClickHandler(*h);
          delete h;
        }) {}

  void operator()() const { ArtisanGoInvokeClickHandler(*handle_); }

private:
  std::shared_ptr<uintptr_t> handle_;
};

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

void ArtisanNodeSetAttribute(ArtisanNode *node, const char *name,
                              const char *value) {
  ToNode(node)->SetAttribute(name, value);
}

void ArtisanNodeRemoveAttribute(ArtisanNode *node, const char *name) {
  ToNode(node)->RemoveAttribute(name);
}

void ArtisanNodeSetOnClick(ArtisanNode *node, uintptr_t handle) {
  ToNode(node)->SetOnClick(GoCallback(handle));
}

void ArtisanFreeString(char *str) { free(str); }

} // extern "C"
