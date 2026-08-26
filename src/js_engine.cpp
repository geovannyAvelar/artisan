#include "js_engine.h"

#include "css.h"

extern "C" {
#include "quickjs.h"
}

#include <SDL2/SDL.h>

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

// A JSContext only has room for one opaque pointer (JS_SetContextOpaque/
// JS_GetContextOpaque) - document.getElementById and setTimeout/
// setInterval both need to reach something engine-wide, so they share
// this one struct instead of fighting over the single slot. Owned by
// JsEngine::Impl, one instance per JsEngine (see JsEngine's constructor).
struct EngineContext {
  Node *document;
  TimerQueue *timers;
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

private:
  JSValue fn_;
};

// Node::EventHandler's JS-side implementation - builds a small
// {type, target} event object (target via WrapExistingNode above) and
// calls the held JS function with it as the sole argument, same shape a
// real addEventListener callback expects.
class JsCallback : public JsFunctionHandle {
public:
  using JsFunctionHandle::JsFunctionHandle;

  void operator()(const Event &event) const {
    JSValue eventObj = JS_NewObject(ctx_);
    JS_SetPropertyStr(ctx_, eventObj, "type",
                       JS_NewString(ctx_, event.type.c_str()));
    JS_SetPropertyStr(ctx_, eventObj, "target",
                       WrapExistingNode(ctx_, event.target));
    JSValueConst argv[] = {eventObj};
    Invoke(1, argv);
    JS_FreeValue(ctx_, eventObj);
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

  // Any type string is accepted - Node itself doesn't predefine which
  // event types exist (see AddEventListener's doc comment in dom_node.h).
  // Whether anything ever actually DispatchEvent(eventType)s is up to
  // main.cpp/artisanc-generated code/this Node model's other callers, not
  // this binding - a listener for a type nothing ever fires just never
  // runs, same as a browser accepting addEventListener for a typo'd
  // event name without complaint.
  node->AddEventListener(eventType, JsCallback(ctx, argv[1]));
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
    JS_CFUNC_DEF("querySelector", 1, JsNodeQuerySelector),
    JS_CFUNC_DEF("querySelectorAll", 1, JsNodeQuerySelectorAll),
    JS_CFUNC_DEF("addEventListener", 2, JsNodeAddEventListener),
    JS_CGETSET_DEF("tagName", JsNodeGetTagName, nullptr),
    JS_CGETSET_DEF("textContent", JsNodeGetTextContent, JsNodeSetTextContent),
    JS_CGETSET_DEF("parentNode", JsNodeGetParentNode, nullptr),
    JS_CGETSET_DEF("nextSibling", JsNodeGetNextSibling, nullptr),
    JS_CGETSET_DEF("previousSibling", JsNodeGetPreviousSibling, nullptr),
    JS_CGETSET_DEF("children", JsNodeGetChildren, nullptr),
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

} // namespace

struct JsEngine::Impl {
  JSRuntime *rt = nullptr;
  JSContext *ctx = nullptr;
  EngineContext engineContext{nullptr, nullptr};

  ~Impl() {
    if (ctx != nullptr) {
      JS_FreeContext(ctx);
    }
    if (rt != nullptr) {
      JS_FreeRuntime(rt);
    }
  }
};

JsEngine::JsEngine(Node &document, TimerQueue &timers)
    : impl_(std::make_unique<Impl>()) {
  impl_->rt = JS_NewRuntime();
  impl_->ctx = JS_NewContext(impl_->rt);

  impl_->engineContext = EngineContext{&document, &timers};
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

  JSValue documentObj = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, documentObj, kDocumentFuncs,
                              static_cast<int>(std::size(kDocumentFuncs)));

  JSValue consoleObj = JS_NewObject(impl_->ctx);
  JS_SetPropertyFunctionList(impl_->ctx, consoleObj, kConsoleFuncs,
                              static_cast<int>(std::size(kConsoleFuncs)));

  JSValue global = JS_GetGlobalObject(impl_->ctx);
  JS_SetPropertyStr(impl_->ctx, global, "document", documentObj);
  JS_SetPropertyStr(impl_->ctx, global, "console", consoleObj);
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
