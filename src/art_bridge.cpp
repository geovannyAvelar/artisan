#include "art_bridge.h"

#include <cstdlib>
#include <cstring>
#include <string>

#ifdef ARTISAN_HAS_ART_GC
#include <gc.h>
#endif

#include "art_bridge_context.h"
#include "dom_node.h"
#include "node_c_api.h"

namespace {

artisan::Node *g_currentDocument = nullptr;

// A heap allocation ART code will end up holding a pointer to needs to
// come from the exact same managed heap ART's own codegen allocates
// from (see art/codegen.cpp's GenHeapAlloc) - otherwise Boehm GC, which
// only ever scans/reclaims memory it allocated itself, would silently
// treat this allocation as permanently outside its accounting and it
// would leak forever regardless of reachability, defeating the whole
// point. ARTISAN_HAS_ART_GC is only defined when an ART app is actually
// configured (see the root CMakeLists.txt) - this file is always
// compiled, even for a project with no ART involved at all, so it can't
// unconditionally depend on libgc-dev the way the ART compiler itself
// can.
void *ArtHeapAlloc(size_t bytes) {
#ifdef ARTISAN_HAS_ART_GC
  return GC_malloc(bytes);
#else
  return std::malloc(bytes);
#endif
}

// Heap-allocates a fresh ART-shaped string (a 16-byte { i64 length, ptr
// data } header pointing at a length+1-byte, null-terminated data buffer)
// from a plain C string - exactly the shape art/codegen.cpp itself builds
// for a string literal, so ART code on the receiving end can't tell the
// difference.
ArtString *MakeArtString(const char *cstr) {
  size_t len = std::strlen(cstr);
  char *data = static_cast<char *>(ArtHeapAlloc(len + 1));
  std::memcpy(data, cstr, len + 1); // include the null terminator
  auto *header = static_cast<ArtString *>(ArtHeapAlloc(sizeof(ArtString)));
  header->length = static_cast<int64_t>(len);
  header->data = data;
  return header;
}

// Heap-boxes a primitive so it can be handed to Node::DispatchEvent's
// `const void*` detail (see the ArtDispatchEvent$.../ArtEventDetail$...
// overloads below).
double *BoxDouble(double value) {
  auto *box = static_cast<double *>(ArtHeapAlloc(sizeof(double)));
  *box = value;
  return box;
}

bool *BoxBool(bool value) {
  auto *box = static_cast<bool *>(ArtHeapAlloc(sizeof(bool)));
  *box = value;
  return box;
}

// Wraps an ArtEventHandler (a raw ART function pointer) so it can be
// stored in a Node::EventHandler (std::function<void(Event&)>) and later
// *recovered* - a plain lambda can't be, since std::function::target<T>()
// needs the exact stored type named, and a lambda's type has no name to
// give it. Same idiom as node_c_api.cpp's GoCallback / js_engine.cpp's
// JsCallback, just wrapping a bare C function pointer instead of a Go
// handle or a JS closure - see ArtRemoveEventListener below, which is
// this class's whole reason to exist (ArtAddEventListener alone could
// have used an anonymous lambda instead).
class ArtEventCallback {
public:
  explicit ArtEventCallback(ArtEventHandler handler) : handler_(handler) {}
  void operator()(artisan::Event &event) const { handler_(&event); }
  ArtEventHandler handler() const { return handler_; }

private:
  ArtEventHandler handler_;
};

} // namespace

namespace artisan {

void SetArtDocumentContext(Node *document) { g_currentDocument = document; }

} // namespace artisan

extern "C" {

void *ArtDocument() { return g_currentDocument; }

void *ArtFindById(void *root, ArtString *id) {
  return ArtisanNodeFindById(static_cast<ArtisanNode *>(root), id->data);
}

void *ArtQuerySelector(void *root, ArtString *selector) {
  return ArtisanQuerySelector(static_cast<ArtisanNode *>(root), selector->data);
}

bool ArtIsNull(void *node) { return node == nullptr; }

ArtString *ArtGetTextContent(void *node) {
  char *text = ArtisanNodeTextContent(static_cast<ArtisanNode *>(node));
  ArtString *result = MakeArtString(text);
  ArtisanFreeString(text);
  return result;
}

void ArtSetTextContent(void *node, ArtString *text) {
  ArtisanNodeSetTextContent(static_cast<ArtisanNode *>(node), text->data);
}

ArtString *ArtGetAttribute(void *node, ArtString *name) {
  char *value = ArtisanNodeGetAttribute(static_cast<ArtisanNode *>(node), name->data);
  ArtString *result = MakeArtString(value != nullptr ? value : "");
  if (value != nullptr) ArtisanFreeString(value);
  return result;
}

bool ArtHasAttribute(void *node, ArtString *name) {
  return ArtisanNodeHasAttribute(static_cast<ArtisanNode *>(node), name->data);
}

void ArtSetAttribute(void *node, ArtString *name, ArtString *value) {
  ArtisanNodeSetAttribute(static_cast<ArtisanNode *>(node), name->data, value->data);
}

double ArtChildCount(void *node) {
  return static_cast<double>(ArtisanNodeChildCount(static_cast<ArtisanNode *>(node)));
}

void *ArtChildAt(void *node, double index) {
  return ArtisanNodeChildAt(static_cast<ArtisanNode *>(node), static_cast<size_t>(index));
}

void ArtSetOnClick(void *node, ArtHandler handler) {
  // Goes straight to Node::SetOnClick rather than through node_c_api.h's
  // ArtisanNodeSetOnClick, which only knows how to wrap a Go-style
  // uintptr_t handle (see GoCallback in node_c_api.cpp) - a bare C
  // function pointer already satisfies ClickHandler (std::function<void()>)
  // directly, no wrapper needed.
  static_cast<artisan::Node *>(node)->SetOnClick(handler);
}

void ArtAddEventListener(void *node, ArtString *eventType, ArtEventHandler handler, bool capture) {
  std::string type(eventType->data, static_cast<size_t>(eventType->length));
  static_cast<artisan::Node *>(node)->AddEventListener(type, ArtEventCallback(handler), capture);
}

void ArtRemoveEventListener(void *node, ArtString *eventType, ArtEventHandler handler, bool capture) {
  std::string type(eventType->data, static_cast<size_t>(eventType->length));
  static_cast<artisan::Node *>(node)->RemoveEventListener(
      type, capture, [handler](const artisan::EventHandler &stored) {
        const ArtEventCallback *cb = stored.target<ArtEventCallback>();
        return cb != nullptr && cb->handler() == handler;
      });
}

bool ArtDispatchEvent$number(void *node, ArtString *eventType, bool bubbles, bool cancelable, double detail) {
  std::string type(eventType->data, static_cast<size_t>(eventType->length));
  return static_cast<artisan::Node *>(node)->DispatchEvent(type, bubbles, cancelable, BoxDouble(detail));
}

bool ArtDispatchEvent$boolean(void *node, ArtString *eventType, bool bubbles, bool cancelable, bool detail) {
  std::string type(eventType->data, static_cast<size_t>(eventType->length));
  return static_cast<artisan::Node *>(node)->DispatchEvent(type, bubbles, cancelable, BoxBool(detail));
}

bool ArtDispatchEvent$string(void *node, ArtString *eventType, bool bubbles, bool cancelable, ArtString *detail) {
  std::string type(eventType->data, static_cast<size_t>(eventType->length));
  // `detail` is already a heap-allocated, never-freed ArtString* - the
  // exact same shape/lifetime every other ART string value has - so it's
  // handed to Node::DispatchEvent's `const void*` as-is, no copy needed;
  // ArtEventDetail$string below just casts it straight back.
  return static_cast<artisan::Node *>(node)->DispatchEvent(type, bubbles, cancelable, detail);
}

double ArtEventDetail$number(void *event) {
  const void *detail = static_cast<artisan::Event *>(event)->detail;
  return detail == nullptr ? 0.0 : *static_cast<const double *>(detail);
}

bool ArtEventDetail$boolean(void *event) {
  const void *detail = static_cast<artisan::Event *>(event)->detail;
  return detail != nullptr && *static_cast<const bool *>(detail);
}

ArtString *ArtEventDetail$string(void *event) {
  const void *detail = static_cast<artisan::Event *>(event)->detail;
  if (detail == nullptr) return MakeArtString("");
  return const_cast<ArtString *>(static_cast<const ArtString *>(detail));
}

ArtString *ArtEventType(void *event) { return MakeArtString(static_cast<artisan::Event *>(event)->type.c_str()); }

void *ArtEventTarget(void *event) { return static_cast<artisan::Event *>(event)->target; }

bool ArtEventBubbles(void *event) { return static_cast<artisan::Event *>(event)->Bubbles(); }

bool ArtEventCancelable(void *event) { return static_cast<artisan::Event *>(event)->Cancelable(); }

void ArtEventPreventDefault(void *event) { static_cast<artisan::Event *>(event)->PreventDefault(); }

bool ArtEventDefaultPrevented(void *event) { return static_cast<artisan::Event *>(event)->DefaultPrevented(); }

void ArtEventStopPropagation(void *event) { static_cast<artisan::Event *>(event)->StopPropagation(); }

void ArtEventStopImmediatePropagation(void *event) {
  static_cast<artisan::Event *>(event)->StopImmediatePropagation();
}

double ArtEventClientX(void *event) { return static_cast<double>(static_cast<artisan::Event *>(event)->clientX); }

double ArtEventClientY(void *event) { return static_cast<double>(static_cast<artisan::Event *>(event)->clientY); }

bool ArtEventCtrlKey(void *event) { return static_cast<artisan::Event *>(event)->ctrlKey; }

bool ArtEventShiftKey(void *event) { return static_cast<artisan::Event *>(event)->shiftKey; }

bool ArtEventAltKey(void *event) { return static_cast<artisan::Event *>(event)->altKey; }

bool ArtEventMetaKey(void *event) { return static_cast<artisan::Event *>(event)->metaKey; }

ArtString *ArtEventKey(void *event) { return MakeArtString(static_cast<artisan::Event *>(event)->key.c_str()); }

ArtString *ArtEventCode(void *event) { return MakeArtString(static_cast<artisan::Event *>(event)->code.c_str()); }

} // extern "C"
