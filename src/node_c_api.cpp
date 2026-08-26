#include "node_c_api.h"

#include "css.h"
#include "dom_node.h"
#include "node_c_api_bridge.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
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
// covers both - it has both a zero-arg operator() (for
// Node::ClickHandler/SetOnClick) and an Event-taking one that just
// ignores the argument (for Node::EventHandler/AddEventListener), rather
// than one of the two call sites wrapping this in an adapter lambda:
// ArtisanNodeRemoveEventListener needs `handler.target<GoCallback>()` to
// actually recover a GoCallback (see JsCallback::Matches in
// js_engine.cpp for the same requirement on the JS side), which only
// works if a GoCallback is literally what got stored in the
// std::function, not some lambda wrapping one.
class GoCallback {
public:
  explicit GoCallback(uintptr_t handle)
      : handle_(new uintptr_t(handle), [](uintptr_t *h) {
          ArtisanGoReleaseHandler(*h);
          delete h;
        }) {}

  void operator()() const { ArtisanGoInvokeHandler(*handle_); }
  void operator()(const artisan::Event &) const { ArtisanGoInvokeHandler(*handle_); }
  void operator()(double timestampMs) const {
    ArtisanGoInvokeAnimationFrameHandler(*handle_, timestampMs);
  }

  // Whether `handle` is the exact value this GoCallback was constructed
  // with - what ArtisanNodeRemoveEventListener's "same handler" matching
  // needs (a cgo.Handle is already a stable, comparable identity, unlike
  // JsCallback::Matches in js_engine.cpp, which has to compare a JSValue
  // by tag+pointer instead).
  bool Matches(uintptr_t handle) const { return *handle_ == handle; }

private:
  std::shared_ptr<uintptr_t> handle_;
};

// classList's "class" attribute token manipulation - a Go-side
// duplicate of js_engine.cpp's SplitClassTokens/JoinClassTokens/
// WriteClassTokens (same reasoning as GoCallback vs JsCallback above:
// small and self-contained enough that keeping the two bindings
// independent beats sharing it through a new css.h entry point).
std::vector<std::string> SplitClassTokens(const std::string &classAttr) {
  std::vector<std::string> tokens;
  std::istringstream ss(classAttr);
  std::string token;
  while (ss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

std::string JoinClassTokens(const std::vector<std::string> &tokens) {
  std::string out;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (i > 0) {
      out += ' ';
    }
    out += tokens[i];
  }
  return out;
}

void WriteClassTokens(Node &node, const std::vector<std::string> &tokens) {
  if (tokens.empty()) {
    node.RemoveAttribute("class");
  } else {
    node.SetAttribute("class", JoinClassTokens(tokens));
  }
}

// dataset's data-* convention - a Go-side duplicate of js_engine.cpp's
// DataNameToAttribute, same reasoning as above.
std::string DataNameToAttribute(const std::string &name) {
  std::string result = "data-";
  for (char c : name) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (std::isupper(uc)) {
      result += '-';
      result += static_cast<char>(std::tolower(uc));
    } else {
      result += c;
    }
  }
  return result;
}

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

// What ArtisanSetTimeout/SetInterval/RequestAnimationFrame schedule
// into - set by SetGoTimerContext (node_c_api_bridge.h) before
// ArtisanSetupApp runs, same "one instance at a time" precedent
// g_pendingNodes above already establishes. nullptr (the pre-SetupApp
// default) makes those three functions safe, id-0-returning no-ops.
artisan::TimerQueue *g_timerQueue = nullptr;
artisan::AnimationFrameQueue *g_animationFrames = nullptr;

} // namespace

namespace artisan {

void SetGoTimerContext(TimerQueue &timers, AnimationFrameQueue &animationFrames) {
  g_timerQueue = &timers;
  g_animationFrames = &animationFrames;
}

} // namespace artisan

extern "C" {

ArtisanNode *ArtisanNodeFindById(ArtisanNode *root, const char *id) {
  return FromNode(ToNode(root)->FindById(id));
}

int ArtisanNodeType(ArtisanNode *node) {
  return ToNode(node)->type() == artisan::NodeType::kElement
             ? ARTISAN_NODE_TYPE_ELEMENT
             : ARTISAN_NODE_TYPE_TEXT;
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

bool ArtisanNodeMatches(ArtisanNode *node, const char *selector) {
  return artisan::ElementMatches(*ToNode(node), selector);
}

ArtisanNode *ArtisanNodeClosest(ArtisanNode *node, const char *selector) {
  return FromNode(artisan::Closest(*ToNode(node), selector));
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

ArtisanNode *ArtisanNodeRemoveChild(ArtisanNode *parent, ArtisanNode *child) {
  std::unique_ptr<Node> removed = ToNode(parent)->RemoveChild(ToNode(child));
  if (!removed) {
    return nullptr;
  }
  return RegisterPending(std::move(removed));
}

ArtisanNode *ArtisanNodeRemove(ArtisanNode *node) {
  std::unique_ptr<Node> removed = ToNode(node)->Remove();
  if (!removed) {
    return nullptr;
  }
  return RegisterPending(std::move(removed));
}

ArtisanNode *ArtisanNodeCloneNode(ArtisanNode *node, bool deep) {
  return RegisterPending(ToNode(node)->CloneNode(deep));
}

void ArtisanNodeAddEventListener(ArtisanNode *node, const char *eventType,
                                  uintptr_t handle, bool capture) {
  ToNode(node)->AddEventListener(eventType, GoCallback(handle), capture);
}

void ArtisanNodeRemoveEventListener(ArtisanNode *node, const char *eventType,
                                     uintptr_t handle, bool capture) {
  ToNode(node)->RemoveEventListener(
      eventType, capture, [handle](const artisan::EventHandler &h) {
        const GoCallback *cb = h.target<GoCallback>();
        return cb != nullptr && cb->Matches(handle);
      });
}

bool ArtisanNodeDispatchEvent(ArtisanNode *node, const char *eventType,
                               bool bubbles, bool cancelable) {
  bool defaultPrevented =
      ToNode(node)->DispatchEvent(eventType, bubbles, cancelable);
  return !defaultPrevented;
}

void ArtisanNodeSetOnClick(ArtisanNode *node, uintptr_t handle) {
  ToNode(node)->SetOnClick(GoCallback(handle));
}

void ArtisanNodeClassListAdd(ArtisanNode *node, const char *name) {
  Node *n = ToNode(node);
  const std::string *classAttr = n->GetAttribute("class");
  std::vector<std::string> tokens =
      classAttr != nullptr ? SplitClassTokens(*classAttr) : std::vector<std::string>();
  if (std::find(tokens.begin(), tokens.end(), name) == tokens.end()) {
    tokens.emplace_back(name);
  }
  WriteClassTokens(*n, tokens);
}

void ArtisanNodeClassListRemove(ArtisanNode *node, const char *name) {
  Node *n = ToNode(node);
  const std::string *classAttr = n->GetAttribute("class");
  if (classAttr == nullptr) {
    return;
  }
  std::vector<std::string> tokens = SplitClassTokens(*classAttr);
  tokens.erase(std::remove(tokens.begin(), tokens.end(), name), tokens.end());
  WriteClassTokens(*n, tokens);
}

bool ArtisanNodeClassListContains(ArtisanNode *node, const char *name) {
  const std::string *classAttr = ToNode(node)->GetAttribute("class");
  if (classAttr == nullptr) {
    return false;
  }
  std::vector<std::string> tokens = SplitClassTokens(*classAttr);
  return std::find(tokens.begin(), tokens.end(), name) != tokens.end();
}

bool ArtisanNodeClassListToggle(ArtisanNode *node, const char *name,
                                 bool hasForce, bool force) {
  Node *n = ToNode(node);
  const std::string *classAttr = n->GetAttribute("class");
  std::vector<std::string> tokens =
      classAttr != nullptr ? SplitClassTokens(*classAttr) : std::vector<std::string>();
  auto it = std::find(tokens.begin(), tokens.end(), name);
  bool present = it != tokens.end();
  bool addIt = hasForce ? force : !present;

  if (addIt && !present) {
    tokens.emplace_back(name);
  } else if (!addIt && present) {
    tokens.erase(it);
  }
  WriteClassTokens(*n, tokens);
  return addIt;
}

char *ArtisanNodeStyleGet(ArtisanNode *node, const char *property) {
  auto value = artisan::GetInlineStyleProperty(*ToNode(node), property);
  return value.has_value() ? CopyString(*value) : nullptr;
}

void ArtisanNodeStyleSet(ArtisanNode *node, const char *property,
                          const char *value) {
  artisan::SetInlineStyleProperty(*ToNode(node), property, value);
}

char *ArtisanNodeGetData(ArtisanNode *node, const char *name) {
  const std::string *value =
      ToNode(node)->GetAttribute(DataNameToAttribute(name));
  return value != nullptr ? CopyString(*value) : nullptr;
}

void ArtisanNodeSetData(ArtisanNode *node, const char *name,
                         const char *value) {
  ToNode(node)->SetAttribute(DataNameToAttribute(name), value);
}

int ArtisanSetTimeout(uintptr_t handle, double delayMs) {
  if (g_timerQueue == nullptr) {
    return 0;
  }
  return g_timerQueue->Schedule(
      GoCallback(handle), SDL_GetTicks(),
      delayMs > 0 ? static_cast<uint32_t>(delayMs) : 0, /*repeating=*/false);
}

int ArtisanSetInterval(uintptr_t handle, double delayMs) {
  if (g_timerQueue == nullptr) {
    return 0;
  }
  return g_timerQueue->Schedule(
      GoCallback(handle), SDL_GetTicks(),
      delayMs > 0 ? static_cast<uint32_t>(delayMs) : 0, /*repeating=*/true);
}

void ArtisanClearTimer(int id) {
  if (g_timerQueue != nullptr) {
    g_timerQueue->Cancel(id);
  }
}

int ArtisanRequestAnimationFrame(uintptr_t handle) {
  if (g_animationFrames == nullptr) {
    return 0;
  }
  return g_animationFrames->Schedule(GoCallback(handle));
}

void ArtisanCancelAnimationFrame(int id) {
  if (g_animationFrames != nullptr) {
    g_animationFrames->Cancel(id);
  }
}

void ArtisanFreeString(char *str) { free(str); }

} // extern "C"
