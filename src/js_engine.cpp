#include "js_engine.h"

#include "css.h"

extern "C" {
#include "quickjs.h"
}

#include <SDL2/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace artisan {

namespace {

// Single JS class ("Node") backing every wrapped artisan::Node - fine as
// a plain global since this process only ever creates one JsEngine (and
// therefore one JSRuntime) at a time.
JSClassID g_nodeClassId = 0;

// Single JS class ("Event") backing the object a listener's callback
// receives - see EventHandle below.
JSClassID g_eventClassId = 0;

// Single JS classes backing node.classList/node.style - see
// ClassListHandle/StyleHandle below.
JSClassID g_classListClassId = 0;
JSClassID g_styleClassId = 0;

// Single JS class ("HTMLCollection") backing node.children - see
// LiveChildrenExotic* below for what makes it live rather than a
// snapshot.
JSClassID g_liveChildrenClassId = 0;

// Single JS class ("DOMStringMap") backing node.dataset - see
// "--- dataset ---" below.
JSClassID g_datasetClassId = 0;

// A JSContext only has room for one opaque pointer (JS_SetContextOpaque/
// JS_GetContextOpaque) - document.getElementById and setTimeout/
// setInterval both need to reach something engine-wide, so they share
// this one struct instead of fighting over the single slot. Owned by
// JsEngine::Impl, one instance per JsEngine (see JsEngine's constructor).
//
// `nodeWrapperCache` is what makes node identity work: WrapExistingNode/
// WrapOwnedNode (below) consult it before building a new "Node" object,
// so two independently-obtained references to the same underlying
// artisan::Node (`document.getElementById(id)` called twice, a
// dispatched event's `.target` vs. a script's own saved reference, ...)
// come back as the *same* JS object - `===` between them now holds. It's
// a weak map, not a normal cache: inserting never bumps the stored
// JSValue's refcount (see CacheNodeWrapper), so a wrapper with no other
// referent still gets garbage collected exactly as before, at which
// point NodeFinalizer erases its entry. The other half of keeping this
// safe is Node::SetDestroyHook (dom_node.h): the *node* side can go away
// first too (a C++/Go caller discarding a RemoveChild return, a whole
// subtree being torn down, ...), and when it does, the hook installed by
// CacheNodeWrapper erases the entry before that address could ever be
// reused by an unrelated later Node - see ~Impl() for why that hook also
// has to be cleared, not just relied on, once this EngineContext itself
// is going away.
struct EngineContext {
  Node *document;
  TimerQueue *timers;
  AnimationFrameQueue *animationFrames;
  std::unordered_map<Node *, JSValue> nodeWrapperCache;
};

EngineContext *ContextOpaque(JSContext *ctx) {
  return static_cast<EngineContext *>(JS_GetContextOpaque(ctx));
}

// A JS "Node" object's opaque data. `ptr` is always the live node this
// wrapper refers to. `owned`, when non-null, means this wrapper currently
// holds the only reference to a *detached* node (just created by
// document.createElement/createTextNode, not yet appended anywhere) - if
// the wrapper is garbage collected while still detached, the node goes
// with it, same as an unreferenced object in a real DOM would.
// appendChild() transfers `owned` into the C++ tree and clears it; `ptr`
// keeps pointing at the same (now tree-owned) node either way.
struct NodeHandle {
  Node *ptr;
  std::unique_ptr<Node> owned;
};

void NodeFinalizer(JSRuntime *rt, JSValueConst val) {
  NodeHandle *handle = static_cast<NodeHandle *>(JS_GetOpaque(val, g_nodeClassId));
  if (handle != nullptr) {
    // Mirror of CacheNodeWrapper's insert: this wrapper is going away, so
    // drop its (weak, non-refcounted) cache entry too - but only if the
    // entry still actually points at *this* object. It normally does;
    // the guard just protects against the (currently never hit, but
    // cheap to guard against) case of some other path having already
    // replaced it with a different wrapper for the same node.
    auto *engine = static_cast<EngineContext *>(JS_GetRuntimeOpaque(rt));
    if (engine != nullptr) {
      auto it = engine->nodeWrapperCache.find(handle->ptr);
      if (it != engine->nodeWrapperCache.end() &&
          JS_VALUE_GET_PTR(it->second) == JS_VALUE_GET_PTR(val)) {
        engine->nodeWrapperCache.erase(it);
      }
    }
  }
  delete handle;
}

// An "Event" JS object's opaque data. `ptr` points at the live
// artisan::Event the current DispatchEvent call owns (a stack-local
// object in dom_node.cpp, never heap/JS-owned) - valid only for the
// duration of the synchronous listener callback that received this
// object. JsCallback nulls it out right after that callback returns
// (see JsCallback::operator() below), so a script that stashes the
// event object and touches it later gets a harmless no-op/undefined
// instead of a dangling-pointer dereference - the event object itself
// stays alive (a script holding a reference keeps its refcount up),
// just permanently inert past that point, closer to how a real
// browser's Event stays a normal object after dispatch (its
// propagation methods just stop mattering) than to a crash.
struct EventHandle {
  Event *ptr;
};

void EventFinalizer(JSRuntime * /*rt*/, JSValueConst val) {
  delete static_cast<EventHandle *>(JS_GetOpaque(val, g_eventClassId));
}

// What artisan::Event::detail (an untyped const void*) actually points
// at whenever this binding sets it: a script-supplied JSValue, plus the
// context that owns it, so a listener's per-callback Event object (see
// JsCallback::operator() below) can dup a live reference to it without
// dom_node.h/.cpp needing to know QuickJS exists. Always stack-local for
// the duration of a single synchronous DispatchEvent call (see
// JsNodeDispatchEvent) - dispatch never outlives the frame that builds
// this, so there's no lifetime issue in reading it back mid-walk.
struct ScriptEventDetail {
  JSContext *ctx;
  JSValueConst value;
};

Event *GetEventPtr(JSContext *ctx, JSValueConst val) {
  EventHandle *handle =
      static_cast<EventHandle *>(JS_GetOpaque2(ctx, val, g_eventClassId));
  return handle != nullptr ? handle->ptr : nullptr;
}

// node.classList/node.style's opaque data - just the Node they were
// created for, non-owning (the underlying Node is tree-owned or held by
// some NodeHandle elsewhere; this object never outlives a single
// `node.classList`/`node.style` expression's worth of use in practice,
// but even if a script stashes it, it's just a dangling-but-never-
// dereferenced-unsafely Node* the same way any other raw pointer into a
// destroyed Node would be - no different a risk than the existing
// wrapper-identity caveat documented on JsEngine itself). Every
// classList/style method re-reads/re-writes the live "class"/"style"
// attribute rather than caching anything, so there's no state here to
// keep in sync in the first place.
struct AttributeViewHandle {
  Node *node;
};

void AttributeViewFinalizer(JSRuntime * /*rt*/, JSValueConst val) {
  delete static_cast<AttributeViewHandle *>(JS_GetOpaque(val, g_classListClassId));
}

void StyleFinalizer(JSRuntime * /*rt*/, JSValueConst val) {
  delete static_cast<AttributeViewHandle *>(JS_GetOpaque(val, g_styleClassId));
}

void LiveChildrenFinalizer(JSRuntime * /*rt*/, JSValueConst val) {
  delete static_cast<AttributeViewHandle *>(
      JS_GetOpaque(val, g_liveChildrenClassId));
}

void DatasetFinalizer(JSRuntime * /*rt*/, JSValueConst val) {
  delete static_cast<AttributeViewHandle *>(JS_GetOpaque(val, g_datasetClassId));
}

Node *GetClassListNode(JSContext *ctx, JSValueConst val) {
  auto *handle = static_cast<AttributeViewHandle *>(
      JS_GetOpaque2(ctx, val, g_classListClassId));
  return handle != nullptr ? handle->node : nullptr;
}

Node *GetStyleNode(JSContext *ctx, JSValueConst val) {
  auto *handle =
      static_cast<AttributeViewHandle *>(JS_GetOpaque2(ctx, val, g_styleClassId));
  return handle != nullptr ? handle->node : nullptr;
}

// Unlike GetClassListNode/GetStyleNode, this is called from exotic
// property hooks - which are also reachable for a plain `JS_GetOpaque`
// failure (e.g. something holding this class's prototype rather than an
// instance), so it uses the non-throwing JS_GetOpaque, not
// JS_GetOpaque2, and lets its caller decide what "not a real instance"
// means for that particular hook.
Node *GetLiveChildrenNode(JSValueConst val) {
  auto *handle =
      static_cast<AttributeViewHandle *>(JS_GetOpaque(val, g_liveChildrenClassId));
  return handle != nullptr ? handle->node : nullptr;
}

// Same non-throwing shape as GetLiveChildrenNode, for the same reason -
// dataset's exotic hooks below.
Node *GetDatasetNode(JSValueConst val) {
  auto *handle =
      static_cast<AttributeViewHandle *>(JS_GetOpaque(val, g_datasetClassId));
  return handle != nullptr ? handle->node : nullptr;
}

void PrintException(JSContext *ctx) {
  JSValue exception = JS_GetException(ctx);
  const char *message = JS_ToCString(ctx, exception);
  std::cerr << "JS error: " << (message != nullptr ? message : "(no message)")
            << "\n";
  JS_FreeCString(ctx, message);
  JS_FreeValue(ctx, exception);
}

// Registers `obj` (a just-built, not-yet-cached "Node" wrapper for
// `node`) in the identity cache - see EngineContext's doc comment for the
// overall scheme. Stores the JSValue without duping it (a weak entry:
// this cache must never be the thing keeping a wrapper alive) and points
// `node`'s destroy hook back at this same cache, so whichever of "the JS
// wrapper gets collected" (NodeFinalizer) or "the node itself gets
// destroyed" (~Node) happens first is the one that cleans the entry up.
void CacheNodeWrapper(JSContext *ctx, Node *node, JSValueConst obj) {
  EngineContext *engine = ContextOpaque(ctx);
  engine->nodeWrapperCache[node] = obj;
  node->SetDestroyHook(
      [engine](Node *n) { engine->nodeWrapperCache.erase(n); });
}

JSValue WrapExistingNode(JSContext *ctx, Node *node) {
  if (node == nullptr) {
    return JS_NULL;
  }
  EngineContext *engine = ContextOpaque(ctx);
  auto it = engine->nodeWrapperCache.find(node);
  if (it != engine->nodeWrapperCache.end()) {
    return JS_DupValue(ctx, it->second);
  }
  JSValue obj = JS_NewObjectClass(ctx, g_nodeClassId);
  if (JS_IsException(obj)) {
    return obj;
  }
  JS_SetOpaque(obj, new NodeHandle{node, nullptr});
  CacheNodeWrapper(ctx, node, obj);
  return obj;
}

JSValue WrapOwnedNode(JSContext *ctx, std::unique_ptr<Node> node) {
  JSValue obj = JS_NewObjectClass(ctx, g_nodeClassId);
  if (JS_IsException(obj)) {
    return obj;
  }
  Node *ptr = node.get();
  JS_SetOpaque(obj, new NodeHandle{ptr, std::move(node)});
  // A brand-new node (createElement/createTextNode/cloneNode) can't
  // already be in the cache - it's a fresh heap allocation - but it
  // still needs to go *into* the cache now, so a later WrapExistingNode
  // for this same node (e.g. document.getElementById after it's been
  // appended and given an id) finds and reuses this exact wrapper
  // instead of minting a second, `===`-unequal one.
  CacheNodeWrapper(ctx, ptr, obj);
  return obj;
}

NodeHandle *GetHandle(JSContext *ctx, JSValueConst val) {
  return static_cast<NodeHandle *>(JS_GetOpaque2(ctx, val, g_nodeClassId));
}

Node *GetNode(JSContext *ctx, JSValueConst val) {
  NodeHandle *handle = GetHandle(ctx, val);
  return handle != nullptr ? handle->ptr : nullptr;
}

// --- Event methods ---

JSValue JsEventPreventDefault(JSContext *ctx, JSValueConst this_val,
                               int /*argc*/, JSValueConst * /*argv*/) {
  Event *event = GetEventPtr(ctx, this_val);
  if (event != nullptr) {
    event->PreventDefault();
  }
  return JS_UNDEFINED;
}

JSValue JsEventStopPropagation(JSContext *ctx, JSValueConst this_val,
                                int /*argc*/, JSValueConst * /*argv*/) {
  Event *event = GetEventPtr(ctx, this_val);
  if (event != nullptr) {
    event->StopPropagation();
  }
  return JS_UNDEFINED;
}

JSValue JsEventStopImmediatePropagation(JSContext *ctx, JSValueConst this_val,
                                         int /*argc*/, JSValueConst * /*argv*/) {
  Event *event = GetEventPtr(ctx, this_val);
  if (event != nullptr) {
    event->StopImmediatePropagation();
  }
  return JS_UNDEFINED;
}

JSValue JsEventGetDefaultPrevented(JSContext *ctx, JSValueConst this_val) {
  Event *event = GetEventPtr(ctx, this_val);
  return JS_NewBool(ctx, event != nullptr && event->DefaultPrevented());
}

const JSCFunctionListEntry kEventProto[] = {
    JS_CFUNC_DEF("preventDefault", 0, JsEventPreventDefault),
    JS_CFUNC_DEF("stopPropagation", 0, JsEventStopPropagation),
    JS_CFUNC_DEF("stopImmediatePropagation", 0, JsEventStopImmediatePropagation),
    JS_CGETSET_DEF("defaultPrevented", JsEventGetDefaultPrevented, nullptr),
};

// `new CustomEvent(type, {detail, bubbles, cancelable})` / `new
// Event(type, {bubbles, cancelable})` - registered as both global names
// under the same function (real Event's init dict just never has a
// `detail` key, so reading one that isn't there below already produces
// the right "no detail" result for a plain `new Event(...)`, same as
// real DOM where CustomEvent is the only one of the two with a detail).
//
// Produces a class-g_eventClassId object exactly like the ones
// JsCallback builds for a listener callback, except its EventHandle
// starts out (and stays) detached (ptr == nullptr) - this object isn't
// live-bound to any in-flight C++ Event the way a listener's callback
// argument is, it's just a data carrier a script can pass to
// node.dispatchEvent(...) (see JsNodeDispatchEvent below), which reads
// type/detail/bubbles/cancelable back off of it. preventDefault()/
// stopPropagation() called on it directly (before or instead of ever
// dispatching it) are therefore safe, harmless no-ops via the same
// GetEventPtr-returns-null path already used for a post-dispatch stashed
// event (see EventHandle's doc comment) - not a special case here.
JSValue JsEventConstructor(JSContext *ctx, JSValueConst /*new_target*/,
                            int argc, JSValueConst *argv) {
  if (argc < 1) {
    JS_ThrowTypeError(ctx, "Event/CustomEvent constructor requires a type argument");
    return JS_EXCEPTION;
  }
  const char *type = JS_ToCString(ctx, argv[0]);
  std::string eventType = type != nullptr ? type : "";
  JS_FreeCString(ctx, type);

  bool bubbles = false;
  bool cancelable = false;
  JSValue detail = JS_NULL;
  if (argc >= 2 && JS_IsObject(argv[1])) {
    JSValue bubblesVal = JS_GetPropertyStr(ctx, argv[1], "bubbles");
    if (!JS_IsUndefined(bubblesVal)) {
      bubbles = JS_ToBool(ctx, bubblesVal) > 0;
    }
    JS_FreeValue(ctx, bubblesVal);

    JSValue cancelableVal = JS_GetPropertyStr(ctx, argv[1], "cancelable");
    if (!JS_IsUndefined(cancelableVal)) {
      cancelable = JS_ToBool(ctx, cancelableVal) > 0;
    }
    JS_FreeValue(ctx, cancelableVal);

    // A new reference (or JS_UNDEFINED if the property truly isn't
    // there) - normalized to JS_NULL either way so `.detail` always
    // reads back as a value, never `undefined`, matching real
    // CustomEvent (whose `detail` defaults to null, not absent).
    JSValue detailVal = JS_GetPropertyStr(ctx, argv[1], "detail");
    if (!JS_IsUndefined(detailVal)) {
      detail = detailVal;
    } else {
      JS_FreeValue(ctx, detailVal);
    }
  }

  JSValue eventObj = JS_NewObjectClass(ctx, g_eventClassId);
  if (JS_IsException(eventObj)) {
    JS_FreeValue(ctx, detail);
    return eventObj;
  }
  JS_SetOpaque(eventObj, new EventHandle{nullptr});
  JS_SetPropertyStr(ctx, eventObj, "type", JS_NewString(ctx, eventType.c_str()));
  JS_SetPropertyStr(ctx, eventObj, "target", JS_NULL);
  JS_SetPropertyStr(ctx, eventObj, "bubbles", JS_NewBool(ctx, bubbles));
  JS_SetPropertyStr(ctx, eventObj, "cancelable", JS_NewBool(ctx, cancelable));
  JS_SetPropertyStr(ctx, eventObj, "detail", detail);
  return eventObj;
}

// A copyable handle to a JS function - the common lifetime/ref-counting
// shape both JsCallback and JsTimerCallback below share (each just calls
// the held function differently: with a constructed event object, or
// with no arguments at all). Copying bumps QuickJS's refcount on the
// underlying JSValue; destruction releases it - so a registered callback
// keeps its JS function alive for exactly as long as something
// references this handle, same as any other QuickJS value.
class JsFunctionHandle {
public:
  JsFunctionHandle(JSContext *ctx, JSValueConst fn)
      : ctx_(ctx), fn_(JS_DupValue(ctx, fn)) {}

  JsFunctionHandle(const JsFunctionHandle &other)
      : ctx_(other.ctx_), fn_(JS_DupValue(other.ctx_, other.fn_)) {}

  JsFunctionHandle &operator=(const JsFunctionHandle &other) {
    if (this != &other) {
      JSValue duped = JS_DupValue(other.ctx_, other.fn_);
      JS_FreeValue(ctx_, fn_);
      ctx_ = other.ctx_;
      fn_ = duped;
    }
    return *this;
  }

  ~JsFunctionHandle() { JS_FreeValue(ctx_, fn_); }

protected:
  // Calls the held function with `argc`/`argv` (0/nullptr for none),
  // printing and swallowing a thrown exception the same way every other
  // entry point into script here does (RunScript, the various Node
  // method bindings) - a callback throwing shouldn't crash the whole app.
  void Invoke(int argc, JSValueConst *argv) const {
    JSValue result = JS_Call(ctx_, fn_, JS_UNDEFINED, argc, argv);
    if (JS_IsException(result)) {
      PrintException(ctx_);
    }
    JS_FreeValue(ctx_, result);
  }

  JSContext *ctx_;
  // protected, not private: JsCallback::Matches (below) needs to compare
  // its own fn_ against another JSValue for removeEventListener's
  // identity check.
  JSValue fn_;
};

// Node::EventHandler's JS-side implementation - builds a real "Event"
// object (preventDefault/stopPropagation/stopImmediatePropagation/
// defaultPrevented, backed by the live artisan::Event via EventHandle -
// see kEventProto above) and calls the held JS function with it as the
// sole argument, same shape a real addEventListener callback expects.
class JsCallback : public JsFunctionHandle {
public:
  using JsFunctionHandle::JsFunctionHandle;

  void operator()(Event &event) const {
    JSValue eventObj = JS_NewObjectClass(ctx_, g_eventClassId);
    auto *handle = new EventHandle{&event};
    JS_SetOpaque(eventObj, handle);
    JS_SetPropertyStr(ctx_, eventObj, "type",
                       JS_NewString(ctx_, event.type.c_str()));
    JS_SetPropertyStr(ctx_, eventObj, "target",
                       WrapExistingNode(ctx_, event.target));
    JS_SetPropertyStr(ctx_, eventObj, "bubbles",
                       JS_NewBool(ctx_, event.Bubbles()));
    JS_SetPropertyStr(ctx_, eventObj, "cancelable",
                       JS_NewBool(ctx_, event.Cancelable()));
    // MouseEvent/KeyboardEvent data - always present (0/false/"" for an
    // event that isn't one of those, same simplification real DOM would
    // call "undefined" for a mismatched event type, not tracked
    // separately here since this Node model has one Event class for
    // everything). See Node::DispatchMouseEvent/DispatchKeyEvent
    // (dom_node.h) for what actually populates these - main.cpp's
    // SDL_MOUSEBUTTONDOWN/SDL_KEYDOWN handling.
    JS_SetPropertyStr(ctx_, eventObj, "clientX", JS_NewFloat64(ctx_, event.clientX));
    JS_SetPropertyStr(ctx_, eventObj, "clientY", JS_NewFloat64(ctx_, event.clientY));
    JS_SetPropertyStr(ctx_, eventObj, "ctrlKey", JS_NewBool(ctx_, event.ctrlKey));
    JS_SetPropertyStr(ctx_, eventObj, "shiftKey", JS_NewBool(ctx_, event.shiftKey));
    JS_SetPropertyStr(ctx_, eventObj, "altKey", JS_NewBool(ctx_, event.altKey));
    JS_SetPropertyStr(ctx_, eventObj, "metaKey", JS_NewBool(ctx_, event.metaKey));
    JS_SetPropertyStr(ctx_, eventObj, "key", JS_NewString(ctx_, event.key.c_str()));
    JS_SetPropertyStr(ctx_, eventObj, "code", JS_NewString(ctx_, event.code.c_str()));
    // Only script-dispatched CustomEvents set Event::detail (see
    // ScriptEventDetail above) - every event this Node model fires
    // internally leaves it null, so `.detail` reads back as null there
    // too, matching real DOM's base Event (no detail at all, as opposed
    // to CustomEvent which always has one, possibly null).
    if (event.detail != nullptr) {
      const auto *detail = static_cast<const ScriptEventDetail *>(event.detail);
      JS_SetPropertyStr(ctx_, eventObj, "detail",
                         JS_DupValue(detail->ctx, detail->value));
    } else {
      JS_SetPropertyStr(ctx_, eventObj, "detail", JS_NULL);
    }
    JSValueConst argv[] = {eventObj};
    Invoke(1, argv);
    // See EventHandle's doc comment: `event` is only valid for this
    // synchronous call - detach it now so a script that stashed eventObj
    // (keeping it alive past this point via its own reference) can't
    // later dereference a dangling stack pointer through it.
    handle->ptr = nullptr;
    JS_FreeValue(ctx_, eventObj);
  }

  // Whether `other` is (JS-value-identity) the same function this
  // handle wraps - what removeEventListener's "same function" matching
  // needs (see JsNodeRemoveEventListener below). QuickJS JSValues for
  // objects/functions are reference-counted heap pointers under the
  // hood, so comparing tag+pointer is the standard way to ask "is this
  // literally the same function value".
  bool Matches(JSValueConst other) const {
    return JS_VALUE_GET_TAG(fn_) == JS_VALUE_GET_TAG(other) &&
           JS_VALUE_GET_PTR(fn_) == JS_VALUE_GET_PTR(other);
  }
};

// TimerQueue::Callback's JS-side implementation - setTimeout/setInterval
// callbacks take no argument in a real browser either, so this is just
// Invoke with nothing to pass.
class JsTimerCallback : public JsFunctionHandle {
public:
  using JsFunctionHandle::JsFunctionHandle;

  void operator()() const { Invoke(0, nullptr); }
};

// AnimationFrameQueue::Callback's JS-side implementation -
// requestAnimationFrame callbacks take one numeric timestamp argument.
class JsRafCallback : public JsFunctionHandle {
public:
  using JsFunctionHandle::JsFunctionHandle;

  void operator()(double timestampMs) const {
    JSValueConst argv[] = {JS_NewFloat64(ctx_, timestampMs)};
    Invoke(1, argv);
  }
};

// --- classList ---

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

// Writes `tokens` back to the node's "class" attribute (removing the
// attribute entirely if empty, rather than leaving `class=""` sitting
// around) - every classList mutation below ends with this.
void WriteClassTokens(Node &node, const std::vector<std::string> &tokens) {
  if (tokens.empty()) {
    node.RemoveAttribute("class");
  } else {
    node.SetAttribute("class", JoinClassTokens(tokens));
  }
}

JSValue JsClassListAdd(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv) {
  Node *node = GetClassListNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  const std::string *classAttr = node->GetAttribute("class");
  std::vector<std::string> tokens =
      classAttr != nullptr ? SplitClassTokens(*classAttr) : std::vector<std::string>();
  for (int i = 0; i < argc; ++i) {
    const char *name = JS_ToCString(ctx, argv[i]);
    if (name != nullptr &&
        std::find(tokens.begin(), tokens.end(), name) == tokens.end()) {
      tokens.emplace_back(name);
    }
    JS_FreeCString(ctx, name);
  }
  WriteClassTokens(*node, tokens);
  return JS_UNDEFINED;
}

JSValue JsClassListRemove(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
  Node *node = GetClassListNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  const std::string *classAttr = node->GetAttribute("class");
  std::vector<std::string> tokens =
      classAttr != nullptr ? SplitClassTokens(*classAttr) : std::vector<std::string>();
  for (int i = 0; i < argc; ++i) {
    const char *name = JS_ToCString(ctx, argv[i]);
    if (name != nullptr) {
      tokens.erase(std::remove(tokens.begin(), tokens.end(), name), tokens.end());
    }
    JS_FreeCString(ctx, name);
  }
  WriteClassTokens(*node, tokens);
  return JS_UNDEFINED;
}

JSValue JsClassListContains(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
  Node *node = GetClassListNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const std::string *classAttr = node->GetAttribute("class");
  const char *name = JS_ToCString(ctx, argv[0]);
  bool contains = false;
  if (classAttr != nullptr && name != nullptr) {
    std::vector<std::string> tokens = SplitClassTokens(*classAttr);
    contains = std::find(tokens.begin(), tokens.end(), name) != tokens.end();
  }
  JS_FreeCString(ctx, name);
  return JS_NewBool(ctx, contains);
}

JSValue JsClassListToggle(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
  Node *node = GetClassListNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *name = JS_ToCString(ctx, argv[0]);
  if (name == nullptr) {
    return JS_FALSE;
  }
  const std::string *classAttr = node->GetAttribute("class");
  std::vector<std::string> tokens =
      classAttr != nullptr ? SplitClassTokens(*classAttr) : std::vector<std::string>();
  auto it = std::find(tokens.begin(), tokens.end(), name);
  bool present = it != tokens.end();

  // A second `force` argument pins the result instead of toggling it -
  // real classList.toggle(name, force) semantics.
  bool addIt = argc >= 2 ? JS_ToBool(ctx, argv[1]) > 0 : !present;

  if (addIt && !present) {
    tokens.emplace_back(name);
  } else if (!addIt && present) {
    tokens.erase(it);
  }
  JS_FreeCString(ctx, name);
  WriteClassTokens(*node, tokens);
  return JS_NewBool(ctx, addIt);
}

JSValue JsClassListGetLength(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetClassListNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  const std::string *classAttr = node->GetAttribute("class");
  int length = classAttr != nullptr
                   ? static_cast<int>(SplitClassTokens(*classAttr).size())
                   : 0;
  return JS_NewInt32(ctx, length);
}

const JSCFunctionListEntry kClassListProto[] = {
    JS_CFUNC_DEF("add", 1, JsClassListAdd),
    JS_CFUNC_DEF("remove", 1, JsClassListRemove),
    JS_CFUNC_DEF("contains", 1, JsClassListContains),
    JS_CFUNC_DEF("toggle", 1, JsClassListToggle),
    JS_CGETSET_DEF("length", JsClassListGetLength, nullptr),
};

// --- children (a live HTMLCollection, not a snapshot) ---
//
// node.children used to just be BuildNodeArray(node->children()) - a
// real JS array, but a snapshot: fixed at the moment of the call, deaf
// to any append/remove that happens afterward, same as querySelectorAll
// (which is correctly a snapshot in real DOM too - it's `children` that
// isn't). This section makes it live instead, by *not* pre-populating
// any index/length data at all: the object below carries only a Node*
// (which child list to read), and every `[i]`/`.length` access re-reads
// node->children() at that moment via QuickJS's exotic-object hooks
// (JSClassExoticMethods) - the same mechanism real engines use for
// String's `str[i]` and Proxy. `Object.keys`, `for...in`, and
// `Array.from` all work off of get_own_property_names below and don't
// need anything else; `for...of`/`.forEach()`/spread do NOT work - those
// need Symbol.iterator, which this class doesn't implement (matching
// real HTMLCollection, which - unlike NodeList - was never iterable
// either, before browsers added it as a later, separate addition).

// Parses `prop` as an array-index property name ("0", "1", "23", ... -
// not "01", "-1", "1.5", or anything non-numeric), the same restricted
// grammar real array-index properties use. Needed because - unlike
// quickjs.c's own exotic classes (js_string_get_own_property and
// friends) - this file has no access to quickjs.c's internal tagged-atom
// fast path (__JS_AtomIsTaggedInt/__JS_AtomToUInt32 aren't part of the
// public quickjs.h), so it goes through the atom's string form instead.
bool AtomToIndex(JSContext *ctx, JSAtom prop, uint32_t *outIndex) {
  const char *str = JS_AtomToCString(ctx, prop);
  if (str == nullptr) {
    return false;
  }
  bool ok = false;
  if (str[0] != '\0' && !(str[0] == '0' && str[1] != '\0')) {
    const char *p = str;
    while (*p != '\0' && std::isdigit(static_cast<unsigned char>(*p))) {
      ++p;
    }
    if (*p == '\0') {
      char *end = nullptr;
      unsigned long value = std::strtoul(str, &end, 10);
      if (*end == '\0' && value <= UINT32_MAX) {
        *outIndex = static_cast<uint32_t>(value);
        ok = true;
      }
    }
  }
  JS_FreeCString(ctx, str);
  return ok;
}

// Exotic get_own_property: only ever claims numeric-index properties -
// "length" deliberately isn't handled here (see kLiveChildrenProto's
// getter below instead) so that a lookup for it, or for anything else
// (a method, Symbol.iterator, ...), falls through to this class's
// prototype the normal way, exactly as if this hook didn't exist for
// that property.
int JsLiveChildrenGetOwnProperty(JSContext *ctx, JSPropertyDescriptor *desc,
                                  JSValueConst obj, JSAtom prop) {
  Node *node = GetLiveChildrenNode(obj);
  if (node == nullptr) {
    return false;
  }
  uint32_t index = 0;
  if (!AtomToIndex(ctx, prop, &index)) {
    return false;
  }
  const auto &kids = node->children();
  if (index >= kids.size()) {
    return false;
  }
  if (desc != nullptr) {
    desc->flags = JS_PROP_ENUMERABLE;
    desc->value = WrapExistingNode(ctx, kids[index].get());
    desc->getter = JS_UNDEFINED;
    desc->setter = JS_UNDEFINED;
  }
  return true;
}

// Exotic get_own_property_names: reports index 0..length-1 as this
// object's own keys - what Object.keys/for-in/Array.from actually walk.
// "length" isn't included, matching real Array (its own `length` is a
// non-enumerable data property, invisible to all three of those) - ours
// lives on the prototype instead (see kLiveChildrenProto), so it's
// already correctly excluded without special-casing it here.
int JsLiveChildrenGetOwnPropertyNames(JSContext *ctx, JSPropertyEnum **ptab,
                                       uint32_t *plen, JSValueConst obj) {
  Node *node = GetLiveChildrenNode(obj);
  size_t count = node != nullptr ? node->children().size() : 0;
  JSPropertyEnum *tab = static_cast<JSPropertyEnum *>(
      js_malloc(ctx, sizeof(JSPropertyEnum) * (count > 0 ? count : 1)));
  if (tab == nullptr) {
    return -1;
  }
  for (size_t i = 0; i < count; ++i) {
    tab[i].is_enumerable = true;
    tab[i].atom = JS_NewAtomUInt32(ctx, static_cast<uint32_t>(i));
  }
  *ptab = tab;
  *plen = static_cast<uint32_t>(count);
  return 0;
}

const JSClassExoticMethods kLiveChildrenExotic = {
    .get_own_property = JsLiveChildrenGetOwnProperty,
    .get_own_property_names = JsLiveChildrenGetOwnPropertyNames,
    // delete_property/define_own_property/has_property/get_property/
    // set_property all left null: nulling delete_property/
    // define_own_property doesn't make index assignment/delete
    // *rejected* so much as *ordinary* - `list[0] = x` falls back to
    // JS_DefineProperty's default path, which (per get_own_property
    // above reporting every index as non-writable/non-configurable)
    // still correctly no-ops rather than actually mutating the tree,
    // same net effect real HTMLCollection has, just arrived at through
    // "can't overwrite a non-writable property" rather than a
    // hand-written rejection.
};

Node *GetLiveChildrenNodeThrowing(JSContext *ctx, JSValueConst val) {
  Node *node = GetLiveChildrenNode(val);
  if (node == nullptr) {
    JS_ThrowTypeError(ctx, "not an HTMLCollection");
  }
  return node;
}

JSValue JsLiveChildrenGetLength(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetLiveChildrenNodeThrowing(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  return JS_NewInt32(ctx, static_cast<int32_t>(node->children().size()));
}

const JSCFunctionListEntry kLiveChildrenProto[] = {
    JS_CGETSET_DEF("length", JsLiveChildrenGetLength, nullptr),
};

// --- style ---

// Index into this list is the "magic" value JS_CGETSET_MAGIC_DEF passes
// each getter/setter below - one shared pair of functions instead of one
// per property. Every property css.h's Declarations/ParseDeclarations
// understands (css.cpp) - real CSS's camelCase JS name mapped to its
// kebab-case attribute-text name.
constexpr const char *kStylePropertyNames[] = {
    "color",          "background-color", "font-weight",    "border-color",
    "border-width",   "width",            "height",         "padding-top",
    "padding-right",  "padding-bottom",   "padding-left",   "margin-top",
    "margin-right",   "margin-bottom",    "margin-left",    "display",
    "flex-direction", "justify-content",  "align-items",    "gap",
    "flex-wrap",      "flex-grow",        "flex-shrink",    "flex-basis",
    "align-content",  "grid-template-columns", "grid-template-rows",
    "grid-template-areas", "grid-area", "grid-column", "grid-row"};

JSValue JsStyleGetProperty(JSContext *ctx, JSValueConst this_val, int magic) {
  Node *node = GetStyleNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  std::optional<std::string> value =
      GetInlineStyleProperty(*node, kStylePropertyNames[magic]);
  return JS_NewString(ctx, value.has_value() ? value->c_str() : "");
}

JSValue JsStyleSetProperty(JSContext *ctx, JSValueConst this_val,
                            JSValueConst val, int magic) {
  Node *node = GetStyleNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  const char *value = JS_ToCString(ctx, val);
  if (value != nullptr) {
    SetInlineStyleProperty(*node, kStylePropertyNames[magic], value);
  }
  JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

JSValue JsStyleGetCssText(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetStyleNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  const std::string *styleAttr = node->GetAttribute("style");
  return JS_NewString(ctx, styleAttr != nullptr ? styleAttr->c_str() : "");
}

JSValue JsStyleSetCssText(JSContext *ctx, JSValueConst this_val, JSValueConst val) {
  Node *node = GetStyleNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  const char *text = JS_ToCString(ctx, val);
  if (text != nullptr && text[0] != '\0') {
    node->SetAttribute("style", text);
  } else {
    node->RemoveAttribute("style");
  }
  JS_FreeCString(ctx, text);
  return JS_UNDEFINED;
}

const JSCFunctionListEntry kStyleProto[] = {
    JS_CGETSET_MAGIC_DEF("color", JsStyleGetProperty, JsStyleSetProperty, 0),
    JS_CGETSET_MAGIC_DEF("backgroundColor", JsStyleGetProperty, JsStyleSetProperty, 1),
    JS_CGETSET_MAGIC_DEF("fontWeight", JsStyleGetProperty, JsStyleSetProperty, 2),
    JS_CGETSET_MAGIC_DEF("borderColor", JsStyleGetProperty, JsStyleSetProperty, 3),
    JS_CGETSET_MAGIC_DEF("borderWidth", JsStyleGetProperty, JsStyleSetProperty, 4),
    JS_CGETSET_MAGIC_DEF("width", JsStyleGetProperty, JsStyleSetProperty, 5),
    JS_CGETSET_MAGIC_DEF("height", JsStyleGetProperty, JsStyleSetProperty, 6),
    JS_CGETSET_MAGIC_DEF("paddingTop", JsStyleGetProperty, JsStyleSetProperty, 7),
    JS_CGETSET_MAGIC_DEF("paddingRight", JsStyleGetProperty, JsStyleSetProperty, 8),
    JS_CGETSET_MAGIC_DEF("paddingBottom", JsStyleGetProperty, JsStyleSetProperty, 9),
    JS_CGETSET_MAGIC_DEF("paddingLeft", JsStyleGetProperty, JsStyleSetProperty, 10),
    JS_CGETSET_MAGIC_DEF("marginTop", JsStyleGetProperty, JsStyleSetProperty, 11),
    JS_CGETSET_MAGIC_DEF("marginRight", JsStyleGetProperty, JsStyleSetProperty, 12),
    JS_CGETSET_MAGIC_DEF("marginBottom", JsStyleGetProperty, JsStyleSetProperty, 13),
    JS_CGETSET_MAGIC_DEF("marginLeft", JsStyleGetProperty, JsStyleSetProperty, 14),
    JS_CGETSET_MAGIC_DEF("display", JsStyleGetProperty, JsStyleSetProperty, 15),
    JS_CGETSET_MAGIC_DEF("flexDirection", JsStyleGetProperty, JsStyleSetProperty, 16),
    JS_CGETSET_MAGIC_DEF("justifyContent", JsStyleGetProperty, JsStyleSetProperty, 17),
    JS_CGETSET_MAGIC_DEF("alignItems", JsStyleGetProperty, JsStyleSetProperty, 18),
    JS_CGETSET_MAGIC_DEF("gap", JsStyleGetProperty, JsStyleSetProperty, 19),
    JS_CGETSET_MAGIC_DEF("flexWrap", JsStyleGetProperty, JsStyleSetProperty, 20),
    JS_CGETSET_MAGIC_DEF("flexGrow", JsStyleGetProperty, JsStyleSetProperty, 21),
    JS_CGETSET_MAGIC_DEF("flexShrink", JsStyleGetProperty, JsStyleSetProperty, 22),
    JS_CGETSET_MAGIC_DEF("flexBasis", JsStyleGetProperty, JsStyleSetProperty, 23),
    JS_CGETSET_MAGIC_DEF("alignContent", JsStyleGetProperty, JsStyleSetProperty, 24),
    JS_CGETSET_MAGIC_DEF("gridTemplateColumns", JsStyleGetProperty, JsStyleSetProperty, 25),
    JS_CGETSET_MAGIC_DEF("gridTemplateRows", JsStyleGetProperty, JsStyleSetProperty, 26),
    JS_CGETSET_MAGIC_DEF("gridTemplateAreas", JsStyleGetProperty, JsStyleSetProperty, 27),
    JS_CGETSET_MAGIC_DEF("gridArea", JsStyleGetProperty, JsStyleSetProperty, 28),
    JS_CGETSET_MAGIC_DEF("gridColumn", JsStyleGetProperty, JsStyleSetProperty, 29),
    JS_CGETSET_MAGIC_DEF("gridRow", JsStyleGetProperty, JsStyleSetProperty, 30),
    JS_CGETSET_DEF("cssText", JsStyleGetCssText, JsStyleSetCssText),
};

// --- Node methods ---

JSValue JsNodeGetAttribute(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *name = JS_ToCString(ctx, argv[0]);
  const std::string *value =
      name != nullptr ? node->GetAttribute(name) : nullptr;
  JS_FreeCString(ctx, name);
  return value != nullptr ? JS_NewString(ctx, value->c_str()) : JS_NULL;
}

JSValue JsNodeSetAttribute(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 2) {
    return JS_EXCEPTION;
  }
  const char *name = JS_ToCString(ctx, argv[0]);
  const char *value = JS_ToCString(ctx, argv[1]);
  if (name != nullptr && value != nullptr) {
    node->SetAttribute(name, value);
  }
  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

JSValue JsNodeHasAttribute(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *name = JS_ToCString(ctx, argv[0]);
  bool has = name != nullptr && node->HasAttribute(name);
  JS_FreeCString(ctx, name);
  return JS_NewBool(ctx, has);
}

JSValue JsNodeRemoveAttribute(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *name = JS_ToCString(ctx, argv[0]);
  if (name != nullptr) {
    node->RemoveAttribute(name);
  }
  JS_FreeCString(ctx, name);
  return JS_UNDEFINED;
}

// Builds a JS array of wrapped nodes - shared by the `children` getter and
// querySelectorAll, both of which hand back a snapshot list rather than a
// live one (see QuerySelector's doc comment in css.h for why).
JSValue BuildNodeArray(JSContext *ctx, const std::vector<Node *> &nodes) {
  JSValue array = JS_NewArray(ctx);
  for (size_t i = 0; i < nodes.size(); ++i) {
    JS_SetPropertyUint32(ctx, array, static_cast<uint32_t>(i),
                          WrapExistingNode(ctx, nodes[i]));
  }
  return array;
}

JSValue JsNodeAppendChild(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
  Node *parent = GetNode(ctx, this_val);
  if (parent == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }

  NodeHandle *childHandle = GetHandle(ctx, argv[0]);
  if (childHandle == nullptr) {
    return JS_EXCEPTION;
  }
  if (!childHandle->owned) {
    JS_ThrowTypeError(
        ctx, "appendChild: node is already attached elsewhere "
             "(re-parenting isn't supported)");
    return JS_EXCEPTION;
  }

  parent->AppendChild(std::move(childHandle->owned));
  return JS_DupValue(ctx, argv[0]); // Real DOM returns the appended child.
}

JSValue JsNodeInsertBefore(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
  Node *parent = GetNode(ctx, this_val);
  if (parent == nullptr || argc < 2) {
    return JS_EXCEPTION;
  }

  NodeHandle *childHandle = GetHandle(ctx, argv[0]);
  if (childHandle == nullptr) {
    return JS_EXCEPTION;
  }
  if (!childHandle->owned) {
    JS_ThrowTypeError(
        ctx, "insertBefore: node is already attached elsewhere "
             "(re-parenting isn't supported)");
    return JS_EXCEPTION;
  }

  // Real insertBefore(node, null) means "append at the end" - JS_IsNull
  // covers a script passing null explicitly; a script that just omits the
  // second argument gets JS_UNDEFINED instead, treated the same way.
  Node *before = (JS_IsNull(argv[1]) || JS_IsUndefined(argv[1]))
                     ? nullptr
                     : GetNode(ctx, argv[1]);

  parent->InsertBefore(std::move(childHandle->owned), before);
  return JS_DupValue(ctx, argv[0]);
}

JSValue JsNodeRemoveChild(JSContext *ctx, JSValueConst this_val,
                           int argc, JSValueConst *argv) {
  Node *parent = GetNode(ctx, this_val);
  if (parent == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  NodeHandle *childHandle = GetHandle(ctx, argv[0]);
  if (childHandle == nullptr) {
    return JS_EXCEPTION;
  }
  // Reuses the exact ownership slot WrapOwnedNode already uses for a
  // freshly-created, not-yet-appended node - see RemoveChild's doc
  // comment in dom_node.h and NodeHandle's above. The now-detached node
  // is re-appendable via the existing, unmodified appendChild/
  // insertBefore, which already just check `owned`.
  std::unique_ptr<Node> removed = parent->RemoveChild(childHandle->ptr);
  if (!removed) {
    JS_ThrowTypeError(ctx, "removeChild: node is not a child of this node");
    return JS_EXCEPTION;
  }
  childHandle->owned = std::move(removed);
  return JS_DupValue(ctx, argv[0]); // Real DOM returns the removed child.
}

JSValue JsNodeRemove(JSContext *ctx, JSValueConst this_val,
                      int /*argc*/, JSValueConst * /*argv*/) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  NodeHandle *handle = GetHandle(ctx, this_val);
  std::unique_ptr<Node> removed = node->Remove();
  if (removed && handle != nullptr) {
    handle->owned = std::move(removed);
  }
  return JS_UNDEFINED;
}

JSValue JsNodeCloneNode(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  bool deep = argc >= 1 && JS_ToBool(ctx, argv[0]) > 0;
  return WrapOwnedNode(ctx, node->CloneNode(deep));
}

JSValue JsNodeMatches(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *selector = JS_ToCString(ctx, argv[0]);
  bool matches = selector != nullptr && ElementMatches(*node, selector);
  JS_FreeCString(ctx, selector);
  return JS_NewBool(ctx, matches);
}

JSValue JsNodeClosest(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *selector = JS_ToCString(ctx, argv[0]);
  Node *found = selector != nullptr ? Closest(*node, selector) : nullptr;
  JS_FreeCString(ctx, selector);
  return WrapExistingNode(ctx, found);
}

JSValue JsNodeGetNodeType(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  return JS_NewInt32(ctx, node->type() == NodeType::kElement ? 1 : 3);
}

JSValue JsNodeGetClassList(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  JSValue obj = JS_NewObjectClass(ctx, g_classListClassId);
  if (JS_IsException(obj)) {
    return obj;
  }
  JS_SetOpaque(obj, new AttributeViewHandle{node});
  return obj;
}

JSValue JsNodeGetStyle(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  JSValue obj = JS_NewObjectClass(ctx, g_styleClassId);
  if (JS_IsException(obj)) {
    return obj;
  }
  JS_SetOpaque(obj, new AttributeViewHandle{node});
  return obj;
}

// dataset's data-* convention - "fooBar" <-> "data-foo-bar". Used both by
// getData/setData below (a method-based way to reach the same thing) and
// by the real `node.dataset.fooBar` property syntax ("--- dataset ---"
// further down).
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

JSValue JsNodeGetData(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *name = JS_ToCString(ctx, argv[0]);
  JSValue result = JS_NULL;
  if (name != nullptr) {
    const std::string *value = node->GetAttribute(DataNameToAttribute(name));
    if (value != nullptr) {
      result = JS_NewString(ctx, value->c_str());
    }
  }
  JS_FreeCString(ctx, name);
  return result;
}

JSValue JsNodeSetData(JSContext *ctx, JSValueConst this_val,
                       int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 2) {
    return JS_EXCEPTION;
  }
  const char *name = JS_ToCString(ctx, argv[0]);
  const char *value = JS_ToCString(ctx, argv[1]);
  if (name != nullptr && value != nullptr) {
    node->SetAttribute(DataNameToAttribute(name), value);
  }
  JS_FreeCString(ctx, name);
  JS_FreeCString(ctx, value);
  return JS_UNDEFINED;
}

// --- dataset (real `node.dataset.fooBar` property syntax) ---
//
// Same exotic-hooks approach "--- children ---" above uses for
// node.children, applied to a different shape: instead of numeric
// indices over a child list, this is named properties over the node's
// `data-*` attributes. Reading `dataset.fooBar` reads attribute
// `data-foo-bar` live (no caching); writing or deleting it writes/
// removes that attribute; enumerating it walks every `data-*` attribute
// the node currently has. Unlike children (read-only, non-configurable
// indices), dataset properties are writable and configurable - real
// dataset supports `dataset.foo = "x"` and `delete dataset.foo`.

// The reverse of DataNameToAttribute above ("data-foo-bar" -> "fooBar")
// - only enumeration (JsDatasetGetOwnPropertyNames below) ever needs
// this direction; a get/set/delete always starts from the camelCase
// name and goes the other way. nullopt for an attribute that isn't a
// `data-*` one at all (not every attribute on a node belongs in
// dataset).
std::optional<std::string> AttributeToDataName(const std::string &attr) {
  constexpr size_t kPrefixLen = 5; // strlen("data-")
  if (attr.compare(0, kPrefixLen, "data-") != 0) {
    return std::nullopt;
  }
  std::string result;
  for (size_t i = kPrefixLen; i < attr.size(); ++i) {
    if (attr[i] == '-' && i + 1 < attr.size()) {
      result += static_cast<char>(std::toupper(static_cast<unsigned char>(attr[i + 1])));
      ++i;
    } else {
      result += attr[i];
    }
  }
  return result;
}

// Converts a property atom (whatever a script wrote as `dataset.xyz` or
// `dataset["xyz"]`) to the attribute name it maps to. Only fails if the
// atom can't be read as a string at all (a symbol atom, in practice) -
// any *string* atom, however unlikely a real dataset key (an empty
// string, "0", ...), still maps to *some* attribute name, exactly as
// real dataset would let you round-trip through.
bool AtomToDataAttribute(JSContext *ctx, JSAtom prop, std::string *outAttr) {
  const char *name = JS_AtomToCString(ctx, prop);
  if (name == nullptr) {
    return false;
  }
  *outAttr = DataNameToAttribute(name);
  JS_FreeCString(ctx, name);
  return true;
}

int JsDatasetGetOwnProperty(JSContext *ctx, JSPropertyDescriptor *desc,
                             JSValueConst obj, JSAtom prop) {
  Node *node = GetDatasetNode(obj);
  if (node == nullptr) {
    return false;
  }
  std::string attr;
  if (!AtomToDataAttribute(ctx, prop, &attr)) {
    return false;
  }
  const std::string *value = node->GetAttribute(attr);
  if (value == nullptr) {
    return false;
  }
  if (desc != nullptr) {
    desc->flags = JS_PROP_ENUMERABLE | JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE;
    desc->value = JS_NewString(ctx, value->c_str());
    desc->getter = JS_UNDEFINED;
    desc->setter = JS_UNDEFINED;
  }
  return true;
}

// `dataset.foo = "bar"` (and Object.defineProperty(dataset, "foo",
// {value: "bar"})) both come through here as a plain data-value
// descriptor - the only shape dataset supports. An accessor descriptor
// (a getter/setter) or a value-less reconfigure isn't something real
// dataset accepts either, so those are rejected the same way
// get_own_property already rejects a name with no matching attribute:
// return false rather than defining anything.
int JsDatasetDefineOwnProperty(JSContext *ctx, JSValueConst this_obj,
                                JSAtom prop, JSValueConst val,
                                JSValueConst /*getter*/,
                                JSValueConst /*setter*/, int flags) {
  Node *node = GetDatasetNode(this_obj);
  if (node == nullptr || !(flags & JS_PROP_HAS_VALUE)) {
    return false;
  }
  std::string attr;
  if (!AtomToDataAttribute(ctx, prop, &attr)) {
    return false;
  }
  const char *text = JS_ToCString(ctx, val);
  if (text == nullptr) {
    return -1; // val's own ToString threw.
  }
  node->SetAttribute(attr, text);
  JS_FreeCString(ctx, text);
  return true;
}

// `delete dataset.foo`. Configurable and always present-or-absent (no
// non-configurable dataset property exists to refuse deleting), so this
// always succeeds - same as deleting any ordinary, possibly-already-
// absent property would.
int JsDatasetDeleteProperty(JSContext *ctx, JSValueConst obj, JSAtom prop) {
  Node *node = GetDatasetNode(obj);
  if (node != nullptr) {
    std::string attr;
    if (AtomToDataAttribute(ctx, prop, &attr)) {
      node->RemoveAttribute(attr);
    }
  }
  return true;
}

int JsDatasetGetOwnPropertyNames(JSContext *ctx, JSPropertyEnum **ptab,
                                  uint32_t *plen, JSValueConst obj) {
  Node *node = GetDatasetNode(obj);
  std::vector<std::string> names;
  if (node != nullptr) {
    for (const auto &entry : node->attributes()) {
      std::optional<std::string> dataName = AttributeToDataName(entry.first);
      if (dataName.has_value()) {
        names.push_back(*dataName);
      }
    }
  }
  JSPropertyEnum *tab = static_cast<JSPropertyEnum *>(
      js_malloc(ctx, sizeof(JSPropertyEnum) * (names.empty() ? 1 : names.size())));
  if (tab == nullptr) {
    return -1;
  }
  for (size_t i = 0; i < names.size(); ++i) {
    tab[i].is_enumerable = true;
    tab[i].atom = JS_NewAtom(ctx, names[i].c_str());
  }
  *ptab = tab;
  *plen = static_cast<uint32_t>(names.size());
  return 0;
}

const JSClassExoticMethods kDatasetExotic = {
    .get_own_property = JsDatasetGetOwnProperty,
    .get_own_property_names = JsDatasetGetOwnPropertyNames,
    .delete_property = JsDatasetDeleteProperty,
    .define_own_property = JsDatasetDefineOwnProperty,
};

JSValue JsNodeGetDataset(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  JSValue obj = JS_NewObjectClass(ctx, g_datasetClassId);
  if (JS_IsException(obj)) {
    return obj;
  }
  JS_SetOpaque(obj, new AttributeViewHandle{node});
  return obj;
}

JSValue JsNodeQuerySelector(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *selector = JS_ToCString(ctx, argv[0]);
  Node *found =
      selector != nullptr ? QuerySelector(*node, selector) : nullptr;
  JS_FreeCString(ctx, selector);
  return WrapExistingNode(ctx, found);
}

JSValue JsNodeQuerySelectorAll(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *selector = JS_ToCString(ctx, argv[0]);
  std::vector<Node *> found =
      selector != nullptr ? QuerySelectorAll(*node, selector)
                           : std::vector<Node *>();
  JS_FreeCString(ctx, selector);
  return BuildNodeArray(ctx, found);
}

// Parses addEventListener/removeEventListener's optional third argument
// - real DOM accepts either a bare boolean (the historical form) or an
// options-shaped object with a `capture` property (the modern form).
// Absent (argc < 3) means false, same as real DOM's default.
bool ParseCaptureArg(JSContext *ctx, JSValueConst arg) {
  if (JS_IsUndefined(arg) || JS_IsNull(arg)) {
    return false;
  }
  if (JS_IsObject(arg) && !JS_IsFunction(ctx, arg)) {
    JSValue captureVal = JS_GetPropertyStr(ctx, arg, "capture");
    bool capture = JS_ToBool(ctx, captureVal) > 0;
    JS_FreeValue(ctx, captureVal);
    return capture;
  }
  return JS_ToBool(ctx, arg) > 0;
}

JSValue JsNodeAddEventListener(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 2) {
    return JS_EXCEPTION;
  }
  if (!JS_IsFunction(ctx, argv[1])) {
    JS_ThrowTypeError(ctx, "addEventListener: listener must be a function");
    return JS_EXCEPTION;
  }

  const char *type = JS_ToCString(ctx, argv[0]);
  std::string eventType = type != nullptr ? type : "";
  JS_FreeCString(ctx, type);
  bool capture = argc >= 3 ? ParseCaptureArg(ctx, argv[2]) : false;

  // Any type string is accepted - Node itself doesn't predefine which
  // event types exist (see AddEventListener's doc comment in dom_node.h).
  // Whether anything ever actually DispatchEvent(eventType)s is up to
  // main.cpp/artisanc-generated code/this Node model's other callers, not
  // this binding - a listener for a type nothing ever fires just never
  // runs, same as a browser accepting addEventListener for a typo'd
  // event name without complaint.
  node->AddEventListener(eventType, JsCallback(ctx, argv[1]), capture);
  return JS_UNDEFINED;
}

JSValue JsNodeRemoveEventListener(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 2) {
    return JS_EXCEPTION;
  }

  const char *type = JS_ToCString(ctx, argv[0]);
  std::string eventType = type != nullptr ? type : "";
  JS_FreeCString(ctx, type);
  bool capture = argc >= 3 ? ParseCaptureArg(ctx, argv[2]) : false;

  JSValueConst fnToRemove = argv[1];
  node->RemoveEventListener(eventType, capture,
                             [fnToRemove](const EventHandler &handler) {
                               const JsCallback *cb =
                                   handler.target<JsCallback>();
                               return cb != nullptr && cb->Matches(fnToRemove);
                             });
  return JS_UNDEFINED;
}

// node.dispatchEvent(event) - `event` must be a real Event/CustomEvent
// object (JS_GetOpaque2 throws its own TypeError otherwise, same
// mechanism GetNode/GetEventPtr already rely on elsewhere in this file).
// Reads type/bubbles/cancelable/detail back off of it (own properties -
// see JsEventConstructor above) and drives them straight into
// Node::DispatchEvent, which does the actual capturing/target/bubbling
// walk exactly as it does for an internally-fired click/change/input.
// Each listener invoked during that walk gets its *own* freshly-built
// event object (JsCallback::operator(), same as always) carrying the
// same type/detail/bubbles/cancelable - not literally `event` itself -
// so identity (`event === receivedEvent` inside a listener) doesn't
// hold, consistent with this binding's existing "fresh wrapper per
// access" tradeoff for nodes (see js_engine.h). preventDefault()/
// stopPropagation() called by a listener still correctly affects this
// one dispatch either way, since every one of those fresh objects is
// backed by the same underlying C++ Event& for the duration of the walk.
JSValue JsNodeDispatchEvent(JSContext *ctx, JSValueConst this_val,
                             int argc, JSValueConst *argv) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }

  JSValueConst eventObj = argv[0];
  if (JS_GetOpaque2(ctx, eventObj, g_eventClassId) == nullptr) {
    return JS_EXCEPTION; // JS_GetOpaque2 already threw a TypeError.
  }

  JSValue typeVal = JS_GetPropertyStr(ctx, eventObj, "type");
  const char *typeStr = JS_ToCString(ctx, typeVal);
  std::string eventType = typeStr != nullptr ? typeStr : "";
  JS_FreeCString(ctx, typeStr);
  JS_FreeValue(ctx, typeVal);

  JSValue bubblesVal = JS_GetPropertyStr(ctx, eventObj, "bubbles");
  bool bubbles = JS_ToBool(ctx, bubblesVal) > 0;
  JS_FreeValue(ctx, bubblesVal);

  JSValue cancelableVal = JS_GetPropertyStr(ctx, eventObj, "cancelable");
  bool cancelable = JS_ToBool(ctx, cancelableVal) > 0;
  JS_FreeValue(ctx, cancelableVal);

  // A new reference, freed right after the (synchronous) dispatch
  // returns - ScriptEventDetail only needs to stay valid for the walk's
  // duration, same as `detail` itself being stack-local here.
  JSValue detailVal = JS_GetPropertyStr(ctx, eventObj, "detail");
  ScriptEventDetail detail{ctx, detailVal};

  // Matches real dispatchEvent(): the node it's called on becomes the
  // event's target, visible on the *original* object too (not just the
  // per-listener ones JsCallback builds) for code that inspects it after
  // dispatchEvent() returns.
  JS_SetPropertyStr(ctx, eventObj, "target", WrapExistingNode(ctx, node));

  bool defaultPrevented =
      node->DispatchEvent(eventType, bubbles, cancelable, &detail);
  JS_FreeValue(ctx, detailVal);

  // Real DOM: returns false if the event is cancelable and some
  // listener called preventDefault(), true otherwise.
  return JS_NewBool(ctx, !defaultPrevented);
}

JSValue JsNodeGetParentNode(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  return WrapExistingNode(ctx, node->parent());
}

JSValue JsNodeGetNextSibling(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  return WrapExistingNode(ctx, node->nextSibling());
}

JSValue JsNodeGetPreviousSibling(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  return WrapExistingNode(ctx, node->previousSibling());
}

JSValue JsNodeGetChildren(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  // A live HTMLCollection (see "--- children ---" above), not a snapshot
  // array - a fresh small wrapper object per access, same simplification
  // node.classList/node.style already make (their doc comment on
  // AttributeViewHandle explains why that's fine): what matters is that
  // *reads through it* stay live, not that repeated `node.children`
  // accesses return the same object.
  JSValue obj = JS_NewObjectClass(ctx, g_liveChildrenClassId);
  if (JS_IsException(obj)) {
    return obj;
  }
  JS_SetOpaque(obj, new AttributeViewHandle{node});
  return obj;
}

JSValue JsNodeGetTagName(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  return JS_NewString(ctx, node->tagName().c_str());
}

JSValue JsNodeGetTextContent(JSContext *ctx, JSValueConst this_val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  return JS_NewString(ctx, node->textContent().c_str());
}

JSValue JsNodeSetTextContent(JSContext *ctx, JSValueConst this_val,
                              JSValueConst val) {
  Node *node = GetNode(ctx, this_val);
  if (node == nullptr) {
    return JS_EXCEPTION;
  }
  const char *text = JS_ToCString(ctx, val);
  if (text != nullptr) {
    node->SetTextContent(text);
  }
  JS_FreeCString(ctx, text);
  return JS_UNDEFINED;
}

const JSCFunctionListEntry kNodeProto[] = {
    JS_CFUNC_DEF("getAttribute", 1, JsNodeGetAttribute),
    JS_CFUNC_DEF("setAttribute", 2, JsNodeSetAttribute),
    JS_CFUNC_DEF("hasAttribute", 1, JsNodeHasAttribute),
    JS_CFUNC_DEF("removeAttribute", 1, JsNodeRemoveAttribute),
    JS_CFUNC_DEF("appendChild", 1, JsNodeAppendChild),
    JS_CFUNC_DEF("insertBefore", 2, JsNodeInsertBefore),
    JS_CFUNC_DEF("removeChild", 1, JsNodeRemoveChild),
    JS_CFUNC_DEF("remove", 0, JsNodeRemove),
    JS_CFUNC_DEF("cloneNode", 0, JsNodeCloneNode),
    JS_CFUNC_DEF("matches", 1, JsNodeMatches),
    JS_CFUNC_DEF("closest", 1, JsNodeClosest),
    JS_CFUNC_DEF("querySelector", 1, JsNodeQuerySelector),
    JS_CFUNC_DEF("querySelectorAll", 1, JsNodeQuerySelectorAll),
    JS_CFUNC_DEF("addEventListener", 2, JsNodeAddEventListener),
    JS_CFUNC_DEF("removeEventListener", 2, JsNodeRemoveEventListener),
    JS_CFUNC_DEF("dispatchEvent", 1, JsNodeDispatchEvent),
    JS_CFUNC_DEF("getData", 1, JsNodeGetData),
    JS_CFUNC_DEF("setData", 2, JsNodeSetData),
    JS_CGETSET_DEF("tagName", JsNodeGetTagName, nullptr),
    JS_CGETSET_DEF("textContent", JsNodeGetTextContent, JsNodeSetTextContent),
    JS_CGETSET_DEF("parentNode", JsNodeGetParentNode, nullptr),
    JS_CGETSET_DEF("nextSibling", JsNodeGetNextSibling, nullptr),
    JS_CGETSET_DEF("previousSibling", JsNodeGetPreviousSibling, nullptr),
    JS_CGETSET_DEF("children", JsNodeGetChildren, nullptr),
    JS_CGETSET_DEF("nodeType", JsNodeGetNodeType, nullptr),
    JS_CGETSET_DEF("classList", JsNodeGetClassList, nullptr),
    JS_CGETSET_DEF("style", JsNodeGetStyle, nullptr),
    JS_CGETSET_DEF("dataset", JsNodeGetDataset, nullptr),
    JS_PROP_INT32_DEF("ELEMENT_NODE", 1, JS_PROP_CONFIGURABLE),
    JS_PROP_INT32_DEF("TEXT_NODE", 3, JS_PROP_CONFIGURABLE),
};

// --- document methods ---

JSValue JsDocumentGetElementById(JSContext *ctx, JSValueConst /*this_val*/,
                                  int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_EXCEPTION;
  }
  Node *root = ContextOpaque(ctx)->document;
  const char *id = JS_ToCString(ctx, argv[0]);
  Node *found =
      (root != nullptr && id != nullptr) ? root->FindById(id) : nullptr;
  JS_FreeCString(ctx, id);
  return WrapExistingNode(ctx, found);
}

JSValue JsDocumentQuerySelector(JSContext *ctx, JSValueConst /*this_val*/,
                                 int argc, JSValueConst *argv) {
  Node *root = ContextOpaque(ctx)->document;
  if (root == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *selector = JS_ToCString(ctx, argv[0]);
  Node *found = selector != nullptr ? QuerySelector(*root, selector) : nullptr;
  JS_FreeCString(ctx, selector);
  return WrapExistingNode(ctx, found);
}

JSValue JsDocumentQuerySelectorAll(JSContext *ctx, JSValueConst /*this_val*/,
                                    int argc, JSValueConst *argv) {
  Node *root = ContextOpaque(ctx)->document;
  if (root == nullptr || argc < 1) {
    return JS_EXCEPTION;
  }
  const char *selector = JS_ToCString(ctx, argv[0]);
  std::vector<Node *> found =
      selector != nullptr ? QuerySelectorAll(*root, selector)
                           : std::vector<Node *>();
  JS_FreeCString(ctx, selector);
  return BuildNodeArray(ctx, found);
}

JSValue JsDocumentCreateElement(JSContext *ctx, JSValueConst /*this_val*/,
                                 int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_EXCEPTION;
  }
  const char *tag = JS_ToCString(ctx, argv[0]);
  std::unique_ptr<Node> node = Node::CreateElement(tag != nullptr ? tag : "");
  JS_FreeCString(ctx, tag);
  return WrapOwnedNode(ctx, std::move(node));
}

JSValue JsDocumentCreateTextNode(JSContext *ctx, JSValueConst /*this_val*/,
                                  int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_EXCEPTION;
  }
  const char *text = JS_ToCString(ctx, argv[0]);
  std::unique_ptr<Node> node = Node::CreateText(text != nullptr ? text : "");
  JS_FreeCString(ctx, text);
  return WrapOwnedNode(ctx, std::move(node));
}

const JSCFunctionListEntry kDocumentFuncs[] = {
    JS_CFUNC_DEF("getElementById", 1, JsDocumentGetElementById),
    JS_CFUNC_DEF("querySelector", 1, JsDocumentQuerySelector),
    JS_CFUNC_DEF("querySelectorAll", 1, JsDocumentQuerySelectorAll),
    JS_CFUNC_DEF("createElement", 1, JsDocumentCreateElement),
    JS_CFUNC_DEF("createTextNode", 1, JsDocumentCreateTextNode),
};

// --- console ---

// Real console.log/warn/error all accept any number of arguments of any
// type and print them space-separated - JS_ToCString on each covers that
// generically (numbers/booleans/objects all stringify through it, same
// as template-literal interpolation would).
void PrintConsoleArgs(JSContext *ctx, std::ostream &out, int argc,
                       JSValueConst *argv) {
  for (int i = 0; i < argc; ++i) {
    if (i > 0) {
      out << ' ';
    }
    const char *text = JS_ToCString(ctx, argv[i]);
    out << (text != nullptr ? text : "");
    JS_FreeCString(ctx, text);
  }
  out << '\n';
}

JSValue JsConsoleLog(JSContext *ctx, JSValueConst /*this_val*/, int argc,
                      JSValueConst *argv) {
  PrintConsoleArgs(ctx, std::cout, argc, argv);
  return JS_UNDEFINED;
}

JSValue JsConsoleError(JSContext *ctx, JSValueConst /*this_val*/, int argc,
                        JSValueConst *argv) {
  PrintConsoleArgs(ctx, std::cerr, argc, argv);
  return JS_UNDEFINED;
}

const JSCFunctionListEntry kConsoleFuncs[] = {
    JS_CFUNC_DEF("log", 0, JsConsoleLog),
    JS_CFUNC_DEF("warn", 0, JsConsoleError),
    JS_CFUNC_DEF("error", 0, JsConsoleError),
};

// --- setTimeout/setInterval ---

JSValue JsSetTimer(JSContext *ctx, JSValueConst /*this_val*/, int argc,
                    JSValueConst *argv, bool repeating) {
  TimerQueue *timers = ContextOpaque(ctx)->timers;
  if (timers == nullptr || argc < 1 || !JS_IsFunction(ctx, argv[0])) {
    return JS_EXCEPTION;
  }
  double delayMs = 0;
  if (argc >= 2) {
    JS_ToFloat64(ctx, &delayMs, argv[1]);
  }
  int id = timers->Schedule(JsTimerCallback(ctx, argv[0]), SDL_GetTicks(),
                             delayMs > 0 ? static_cast<uint32_t>(delayMs) : 0,
                             repeating);
  return JS_NewInt32(ctx, id);
}

JSValue JsSetTimeout(JSContext *ctx, JSValueConst this_val, int argc,
                      JSValueConst *argv) {
  return JsSetTimer(ctx, this_val, argc, argv, /*repeating=*/false);
}

JSValue JsSetInterval(JSContext *ctx, JSValueConst this_val, int argc,
                       JSValueConst *argv) {
  return JsSetTimer(ctx, this_val, argc, argv, /*repeating=*/true);
}

JSValue JsClearTimer(JSContext *ctx, JSValueConst /*this_val*/, int argc,
                      JSValueConst *argv) {
  TimerQueue *timers = ContextOpaque(ctx)->timers;
  if (timers == nullptr || argc < 1) {
    return JS_UNDEFINED;
  }
  int32_t id = 0;
  JS_ToInt32(ctx, &id, argv[0]);
  timers->Cancel(id);
  return JS_UNDEFINED;
}

// --- requestAnimationFrame/cancelAnimationFrame ---

JSValue JsRequestAnimationFrame(JSContext *ctx, JSValueConst /*this_val*/,
                                 int argc, JSValueConst *argv) {
  AnimationFrameQueue *frames = ContextOpaque(ctx)->animationFrames;
  if (frames == nullptr || argc < 1 || !JS_IsFunction(ctx, argv[0])) {
    return JS_EXCEPTION;
  }
  int id = frames->Schedule(JsRafCallback(ctx, argv[0]));
  return JS_NewInt32(ctx, id);
}

JSValue JsCancelAnimationFrame(JSContext *ctx, JSValueConst /*this_val*/,
                                int argc, JSValueConst *argv) {
  AnimationFrameQueue *frames = ContextOpaque(ctx)->animationFrames;
  if (frames == nullptr || argc < 1) {
    return JS_UNDEFINED;
  }
  int32_t id = 0;
  JS_ToInt32(ctx, &id, argv[0]);
  frames->Cancel(id);
  return JS_UNDEFINED;
}

} // namespace

struct JsEngine::Impl {
  JSRuntime *rt = nullptr;
  JSContext *ctx = nullptr;
  EngineContext engineContext{nullptr, nullptr, nullptr, {}};

  ~Impl() {
    if (ctx != nullptr) {
      JS_FreeContext(ctx);
    }
    if (rt != nullptr) {
      JS_FreeRuntime(rt);
    }
    // Freeing the context/runtime above finalizes every JS-side wrapper
    // that was still alive, which (via NodeFinalizer) already erased its
    // own cache entry. Anything left in nodeWrapperCache now belongs to a
    // node with no surviving wrapper - i.e. a node the C++ tree itself
    // still owns, which per JsEngine's constructor contract can outlive
    // this JsEngine. Its destroy hook closes over `engineContext`
    // (CacheNodeWrapper), which is about to be destroyed along with the
    // rest of this Impl - clear the hook now so that node's *real*
    // destruction, whenever it eventually happens, doesn't call back into
    // freed memory.
    for (auto &entry : engineContext.nodeWrapperCache) {
      entry.first->SetDestroyHook(nullptr);
    }
  }
};

JsEngine::JsEngine(Node &document, TimerQueue &timers,
                    AnimationFrameQueue &animationFrames)
    : impl_(std::make_unique<Impl>()) {
  impl_->rt = JS_NewRuntime();
  impl_->ctx = JS_NewContext(impl_->rt);

  impl_->engineContext.document = &document;
  impl_->engineContext.timers = &timers;
  impl_->engineContext.animationFrames = &animationFrames;
  JS_SetContextOpaque(impl_->ctx, &impl_->engineContext);
  // NodeFinalizer runs with only the JSRuntime, not this JSContext (see
  // its doc comment) - give it a second way to reach the same
  // EngineContext (and therefore nodeWrapperCache) so it can keep the
  // identity cache in sync with the JS heap's own GC.
  JS_SetRuntimeOpaque(impl_->rt, &impl_->engineContext);

  JS_NewClassID(impl_->rt, &g_nodeClassId);
  JSClassDef nodeClassDef{};
  nodeClassDef.class_name = "Node";
  nodeClassDef.finalizer = NodeFinalizer;
  JS_NewClass(impl_->rt, g_nodeClassId, &nodeClassDef);

  JSValue nodeProto = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, nodeProto, kNodeProto,
                              static_cast<int>(std::size(kNodeProto)));
  JS_SetClassProto(impl_->ctx, g_nodeClassId, nodeProto);

  JS_NewClassID(impl_->rt, &g_eventClassId);
  JSClassDef eventClassDef{};
  eventClassDef.class_name = "Event";
  eventClassDef.finalizer = EventFinalizer;
  JS_NewClass(impl_->rt, g_eventClassId, &eventClassDef);

  JSValue eventProto = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, eventProto, kEventProto,
                              static_cast<int>(std::size(kEventProto)));
  JS_SetClassProto(impl_->ctx, g_eventClassId, eventProto);

  JS_NewClassID(impl_->rt, &g_classListClassId);
  JSClassDef classListClassDef{};
  classListClassDef.class_name = "DOMTokenList";
  classListClassDef.finalizer = AttributeViewFinalizer;
  JS_NewClass(impl_->rt, g_classListClassId, &classListClassDef);

  JSValue classListProto = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, classListProto, kClassListProto,
                              static_cast<int>(std::size(kClassListProto)));
  JS_SetClassProto(impl_->ctx, g_classListClassId, classListProto);

  JS_NewClassID(impl_->rt, &g_styleClassId);
  JSClassDef styleClassDef{};
  styleClassDef.class_name = "CSSStyleDeclaration";
  styleClassDef.finalizer = StyleFinalizer;
  JS_NewClass(impl_->rt, g_styleClassId, &styleClassDef);

  JSValue styleProto = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, styleProto, kStyleProto,
                              static_cast<int>(std::size(kStyleProto)));
  JS_SetClassProto(impl_->ctx, g_styleClassId, styleProto);

  JS_NewClassID(impl_->rt, &g_liveChildrenClassId);
  JSClassDef liveChildrenClassDef{};
  liveChildrenClassDef.class_name = "HTMLCollection";
  liveChildrenClassDef.finalizer = LiveChildrenFinalizer;
  liveChildrenClassDef.exotic =
      const_cast<JSClassExoticMethods *>(&kLiveChildrenExotic);
  JS_NewClass(impl_->rt, g_liveChildrenClassId, &liveChildrenClassDef);

  JSValue liveChildrenProto = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, liveChildrenProto, kLiveChildrenProto,
                              static_cast<int>(std::size(kLiveChildrenProto)));
  JS_SetClassProto(impl_->ctx, g_liveChildrenClassId, liveChildrenProto);

  JS_NewClassID(impl_->rt, &g_datasetClassId);
  JSClassDef datasetClassDef{};
  datasetClassDef.class_name = "DOMStringMap";
  datasetClassDef.finalizer = DatasetFinalizer;
  datasetClassDef.exotic = const_cast<JSClassExoticMethods *>(&kDatasetExotic);
  JS_NewClass(impl_->rt, g_datasetClassId, &datasetClassDef);

  // No own methods/getters - every dataset property is a named `data-*`
  // attribute handled entirely by the exotic hooks above, so the
  // prototype just needs to exist (JS_NewObjectClass requires a class to
  // have one), not carry anything.
  JSValue datasetProto = JS_NewObject(impl_->ctx);
  JS_SetClassProto(impl_->ctx, g_datasetClassId, datasetProto);

  JSValue documentObj = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, documentObj, kDocumentFuncs,
                              static_cast<int>(std::size(kDocumentFuncs)));

  JSValue consoleObj = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, consoleObj, kConsoleFuncs,
                              static_cast<int>(std::size(kConsoleFuncs)));

  // A minimal global "Node" - just ELEMENT_NODE/TEXT_NODE, for code that
  // checks `node.nodeType === Node.ELEMENT_NODE` rather than the literal
  // number (nodeProto's own copies of these two, above, already cover
  // `someNode.ELEMENT_NODE` via the prototype chain either way). Not a
  // real constructor - `new Node()` isn't meaningful here, so it isn't
  // supported.
  JSValue nodeGlobal = JS_NewObject(impl_->ctx);
  JS_SetPropertyStr(impl_->ctx, nodeGlobal, "ELEMENT_NODE", JS_NewInt32(impl_->ctx, 1));
  JS_SetPropertyStr(impl_->ctx, nodeGlobal, "TEXT_NODE", JS_NewInt32(impl_->ctx, 3));

  JSValue global = JS_GetGlobalObject(impl_->ctx);
  JS_SetPropertyStr(impl_->ctx, global, "document", documentObj);
  JS_SetPropertyStr(impl_->ctx, global, "console", consoleObj);
  JS_SetPropertyStr(impl_->ctx, global, "Node", nodeGlobal);
  // setTimeout/setInterval are bare globals (window.setTimeout in a real
  // browser is reachable unqualified the same way), not document methods.
  JS_SetPropertyStr(impl_->ctx, global, "setTimeout",
                     JS_NewCFunction(impl_->ctx, JsSetTimeout, "setTimeout", 2));
  JS_SetPropertyStr(impl_->ctx, global, "setInterval",
                     JS_NewCFunction(impl_->ctx, JsSetInterval, "setInterval", 2));
  JS_SetPropertyStr(impl_->ctx, global, "clearTimeout",
                     JS_NewCFunction(impl_->ctx, JsClearTimer, "clearTimeout", 1));
  JS_SetPropertyStr(impl_->ctx, global, "clearInterval",
                     JS_NewCFunction(impl_->ctx, JsClearTimer, "clearInterval", 1));
  JS_SetPropertyStr(impl_->ctx, global, "requestAnimationFrame",
                     JS_NewCFunction(impl_->ctx, JsRequestAnimationFrame,
                                      "requestAnimationFrame", 1));
  JS_SetPropertyStr(impl_->ctx, global, "cancelAnimationFrame",
                     JS_NewCFunction(impl_->ctx, JsCancelAnimationFrame,
                                      "cancelAnimationFrame", 1));
  // Same underlying constructor for both names - see JsEventConstructor's
  // doc comment for why that's exactly right rather than a shortcut.
  JS_SetPropertyStr(impl_->ctx, global, "Event",
                     JS_NewCFunction2(impl_->ctx, JsEventConstructor, "Event",
                                       2, JS_CFUNC_constructor, 0));
  JS_SetPropertyStr(impl_->ctx, global, "CustomEvent",
                     JS_NewCFunction2(impl_->ctx, JsEventConstructor,
                                       "CustomEvent", 2, JS_CFUNC_constructor,
                                       0));
  JS_FreeValue(impl_->ctx, global);
}

JsEngine::~JsEngine() = default;

bool JsEngine::RunScript(const std::string &source, const std::string &name) {
  JSValue result = JS_Eval(impl_->ctx, source.c_str(), source.size(),
                            name.c_str(), JS_EVAL_TYPE_GLOBAL);
  bool ok = !JS_IsException(result);
  if (!ok) {
    PrintException(impl_->ctx);
  }
  JS_FreeValue(impl_->ctx, result);
  return ok;
}

bool JsEngine::PumpJobQueue() {
  bool ranAny = false;
  for (;;) {
    JSContext *jobCtx = nullptr;
    int status = JS_ExecutePendingJob(impl_->rt, &jobCtx);
    if (status == 0) {
      return ranAny; // No more pending jobs.
    }
    ranAny = true;
    if (status < 0) {
      // jobCtx is whichever context the failing job actually ran
      // against - always impl_->ctx here (this process only ever has
      // the one JSContext this JsEngine created), but PrintException
      // takes whatever JS_ExecutePendingJob reports rather than
      // assuming that.
      PrintException(jobCtx);
    }
  }
}

} // namespace artisan
