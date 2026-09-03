#include "art_bridge.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include <SDL2/SDL.h>

#ifdef ARTISAN_HAS_ART_GC
#include <gc.h>
#endif

#include "animation_frame_queue.h"
#include "art_bridge_context.h"
#include "dom_node.h"
#include "node_c_api.h"
#include "timer_queue.h"

namespace {

artisan::Node *g_currentDocument = nullptr;

// What ArtSetTimeout/ArtSetInterval/ArtRequestAnimationFrame schedule
// into - set by SetArtTimerContext (art_bridge_context.h) before
// setupApp/any handler can possibly run. nullptr (the pre-navigate()
// default) makes those functions safe, id-0-returning no-ops, the same
// tolerance g_currentDocument above already has.
artisan::TimerQueue *g_timerQueue = nullptr;
artisan::AnimationFrameQueue *g_animationFrames = nullptr;

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

// Wraps an ArtEventHandler + its captured env (a raw ART function
// pointer, plus whatever closure environment it needs - see
// include/art_bridge.h's own ArtHandler/ArtEventHandler doc comment) so
// it can be stored in a Node::EventHandler (std::function<void(Event&)>)
// and later *recovered* - a plain lambda can't be, since
// std::function::target<T>() needs the exact stored type named, and a
// lambda's type has no name to give it. Same idiom as node_c_api.cpp's
// GoCallback / js_engine.cpp's JsCallback, just wrapping a bare C
// function pointer (plus env) instead of a Go handle or a JS closure -
// see ArtRemoveEventListener below, which is this class's whole reason
// to exist (ArtAddEventListener alone could have used an anonymous
// lambda instead).
class ArtEventCallback {
public:
  ArtEventCallback(ArtEventHandler handler, void *env) : handler_(handler), env_(env) {}
  void operator()(artisan::Event &event) const { handler_(env_, &event); }
  ArtEventHandler handler() const { return handler_; }
  void *env() const { return env_; }

private:
  ArtEventHandler handler_;
  void *env_;
};

} // namespace

namespace artisan {

void SetArtDocumentContext(Node *document) { g_currentDocument = document; }

void SetArtTimerContext(TimerQueue &timers, AnimationFrameQueue &animationFrames) {
  g_timerQueue = &timers;
  g_animationFrames = &animationFrames;
}

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

void *ArtCreateElement(ArtString *tag) { return ArtisanCreateElement(tag->data); }

void *ArtCreateTextNode(ArtString *text) { return ArtisanCreateTextNode(text->data); }

void *ArtAppendChild(void *parent, void *child) {
  return ArtisanNodeAppendChild(static_cast<ArtisanNode *>(parent), static_cast<ArtisanNode *>(child));
}

void *ArtInsertBefore(void *parent, void *child, void *before) {
  return ArtisanNodeInsertBefore(static_cast<ArtisanNode *>(parent), static_cast<ArtisanNode *>(child),
                                  static_cast<ArtisanNode *>(before));
}

void *ArtRemoveChild(void *parent, void *child) {
  return ArtisanNodeRemoveChild(static_cast<ArtisanNode *>(parent), static_cast<ArtisanNode *>(child));
}

void *ArtRemove(void *node) { return ArtisanNodeRemove(static_cast<ArtisanNode *>(node)); }

void *ArtCloneNode(void *node, bool deep) {
  return ArtisanNodeCloneNode(static_cast<ArtisanNode *>(node), deep);
}

void ArtClassListAdd(void *node, ArtString *name) {
  ArtisanNodeClassListAdd(static_cast<ArtisanNode *>(node), name->data);
}

void ArtClassListRemove(void *node, ArtString *name) {
  ArtisanNodeClassListRemove(static_cast<ArtisanNode *>(node), name->data);
}

bool ArtClassListContains(void *node, ArtString *name) {
  return ArtisanNodeClassListContains(static_cast<ArtisanNode *>(node), name->data);
}

bool ArtClassListToggle(void *node, ArtString *name, bool hasForce, bool force) {
  return ArtisanNodeClassListToggle(static_cast<ArtisanNode *>(node), name->data, hasForce, force);
}

ArtString *ArtGetStyle(void *node, ArtString *property) {
  char *value = ArtisanNodeStyleGet(static_cast<ArtisanNode *>(node), property->data);
  ArtString *result = MakeArtString(value != nullptr ? value : "");
  if (value != nullptr) ArtisanFreeString(value);
  return result;
}

void ArtSetStyle(void *node, ArtString *property, ArtString *value) {
  ArtisanNodeStyleSet(static_cast<ArtisanNode *>(node), property->data, value->data);
}

void ArtSetOnClick(void *node, ArtHandler handler, void *env) {
  // Goes straight to Node::SetOnClick rather than through node_c_api.h's
  // ArtisanNodeSetOnClick, which only knows how to wrap a Go-style
  // uintptr_t handle (see GoCallback in node_c_api.cpp). No identity-
  // recovery need here (unlike ArtAddEventListener below) - SetOnClick
  // always just replaces whatever was there - so a plain capturing
  // lambda is enough, no named wrapper class needed.
  static_cast<artisan::Node *>(node)->SetOnClick([handler, env]() { handler(env); });
}

void ArtAddEventListener(void *node, ArtString *eventType, ArtEventHandler handler, void *env, bool capture) {
  std::string type(eventType->data, static_cast<size_t>(eventType->length));
  static_cast<artisan::Node *>(node)->AddEventListener(type, ArtEventCallback(handler, env), capture);
}

void ArtRemoveEventListener(void *node, ArtString *eventType, ArtEventHandler handler, void *env, bool capture) {
  std::string type(eventType->data, static_cast<size_t>(eventType->length));
  static_cast<artisan::Node *>(node)->RemoveEventListener(
      type, capture, [handler, env](const artisan::EventHandler &stored) {
        const ArtEventCallback *cb = stored.target<ArtEventCallback>();
        // BOTH must match, not just `handler` - see ArtRemoveEventListener's
        // own doc comment in art_bridge.h.
        return cb != nullptr && cb->handler() == handler && cb->env() == env;
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

double ArtSetTimeout(ArtHandler callback, void *env, double delayMs) {
  if (g_timerQueue == nullptr) return 0;
  return static_cast<double>(g_timerQueue->Schedule(
      [callback, env]() { callback(env); }, SDL_GetTicks(),
      delayMs > 0 ? static_cast<uint32_t>(delayMs) : 0, /*repeating=*/false));
}

double ArtSetInterval(ArtHandler callback, void *env, double delayMs) {
  if (g_timerQueue == nullptr) return 0;
  return static_cast<double>(g_timerQueue->Schedule(
      [callback, env]() { callback(env); }, SDL_GetTicks(),
      delayMs > 0 ? static_cast<uint32_t>(delayMs) : 0, /*repeating=*/true));
}

void ArtClearTimer(double id) {
  if (g_timerQueue != nullptr) g_timerQueue->Cancel(static_cast<int>(id));
}

double ArtRequestAnimationFrame(ArtAnimationFrameHandler callback, void *env) {
  if (g_animationFrames == nullptr) return 0;
  return static_cast<double>(g_animationFrames->Schedule([callback, env](double ts) { callback(env, ts); }));
}

void ArtCancelAnimationFrame(double id) {
  if (g_animationFrames != nullptr) g_animationFrames->Cancel(static_cast<int>(id));
}

} // extern "C"
