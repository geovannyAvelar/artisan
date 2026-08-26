#include "js_engine.h"

#include "css.h"

extern "C" {
#include "quickjs.h"
}

#include <SDL2/SDL.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <sstream>
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

// A JSContext only has room for one opaque pointer (JS_SetContextOpaque/
// JS_GetContextOpaque) - document.getElementById and setTimeout/
// setInterval both need to reach something engine-wide, so they share
// this one struct instead of fighting over the single slot. Owned by
// JsEngine::Impl, one instance per JsEngine (see JsEngine's constructor).
struct EngineContext {
  Node *document;
  TimerQueue *timers;
  AnimationFrameQueue *animationFrames;
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

void NodeFinalizer(JSRuntime * /*rt*/, JSValueConst val) {
  delete static_cast<NodeHandle *>(JS_GetOpaque(val, g_nodeClassId));
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

void PrintException(JSContext *ctx) {
  JSValue exception = JS_GetException(ctx);
  const char *message = JS_ToCString(ctx, exception);
  std::cerr << "JS error: " << (message != nullptr ? message : "(no message)")
            << "\n";
  JS_FreeCString(ctx, message);
  JS_FreeValue(ctx, exception);
}

JSValue WrapExistingNode(JSContext *ctx, Node *node) {
  if (node == nullptr) {
    return JS_NULL;
  }
  JSValue obj = JS_NewObjectClass(ctx, g_nodeClassId);
  if (JS_IsException(obj)) {
    return obj;
  }
  JS_SetOpaque(obj, new NodeHandle{node, nullptr});
  return obj;
}

JSValue WrapOwnedNode(JSContext *ctx, std::unique_ptr<Node> node) {
  JSValue obj = JS_NewObjectClass(ctx, g_nodeClassId);
  if (JS_IsException(obj)) {
    return obj;
  }
  Node *ptr = node.get();
  JS_SetOpaque(obj, new NodeHandle{ptr, std::move(node)});
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

// --- style ---

// Index into this list is the "magic" value JS_CGETSET_MAGIC_DEF passes
// each getter/setter below - one shared pair of functions instead of
// five near-identical ones. Exactly the five properties a <style> block
// supports (see css.h/ParseDeclarations) - real CSS's camelCase JS name
// mapped to its kebab-case attribute-text name.
constexpr const char *kStylePropertyNames[] = {
    "color", "background-color", "font-weight", "border-color", "border-width"};

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

// dataset's data-* convention, method-based rather than true
// `dataset.fooBar` property syntax (see js_engine.h's doc comment for
// why) - "fooBar" <-> "data-foo-bar".
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
  std::vector<Node *> children;
  for (const auto &child : node->children()) {
    children.push_back(child.get());
  }
  return BuildNodeArray(ctx, children);
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
  EngineContext engineContext{nullptr, nullptr, nullptr};

  ~Impl() {
    if (ctx != nullptr) {
      JS_FreeContext(ctx);
    }
    if (rt != nullptr) {
      JS_FreeRuntime(rt);
    }
  }
};

JsEngine::JsEngine(Node &document, TimerQueue &timers,
                    AnimationFrameQueue &animationFrames)
    : impl_(std::make_unique<Impl>()) {
  impl_->rt = JS_NewRuntime();
  impl_->ctx = JS_NewContext(impl_->rt);

  impl_->engineContext = EngineContext{&document, &timers, &animationFrames};
  JS_SetContextOpaque(impl_->ctx, &impl_->engineContext);

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

} // namespace artisan
