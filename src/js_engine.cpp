#include "js_engine.h"

extern "C" {
#include "quickjs.h"
}

#include <cstddef>
#include <iostream>
#include <iterator>
#include <utility>

namespace artisan {

namespace {

// Single JS class ("Node") backing every wrapped artisan::Node - fine as
// a plain global since this process only ever creates one JsEngine (and
// therefore one JSRuntime) at a time.
JSClassID g_nodeClassId = 0;

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

// A copyable handle to a JS function, for Node::ClickHandler
// (std::function<void()>, which type-erases via a copyable target).
// Copying bumps QuickJS's refcount on the underlying JSValue; destruction
// releases it - so a click handler keeps its JS callback alive for
// exactly as long as something references this handler, same as any
// other QuickJS value.
class JsCallback {
public:
  JsCallback(JSContext *ctx, JSValueConst fn)
      : ctx_(ctx), fn_(JS_DupValue(ctx, fn)) {}

  JsCallback(const JsCallback &other)
      : ctx_(other.ctx_), fn_(JS_DupValue(other.ctx_, other.fn_)) {}

  JsCallback &operator=(const JsCallback &other) {
    if (this != &other) {
      JSValue duped = JS_DupValue(other.ctx_, other.fn_);
      JS_FreeValue(ctx_, fn_);
      ctx_ = other.ctx_;
      fn_ = duped;
    }
    return *this;
  }

  ~JsCallback() { JS_FreeValue(ctx_, fn_); }

  void operator()() const {
    JSValue result = JS_Call(ctx_, fn_, JS_UNDEFINED, 0, nullptr);
    if (JS_IsException(result)) {
      PrintException(ctx_);
    }
    JS_FreeValue(ctx_, result);
  }

private:
  JSContext *ctx_;
  JSValue fn_;
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

  // Node only has a click hook today (SetOnClick) - anything else is
  // silently accepted but never fires, rather than throwing, so a script
  // using a not-yet-supported event type degrades quietly instead of
  // crashing the whole app.
  if (eventType == "click") {
    node->SetOnClick(JsCallback(ctx, argv[1]));
  }
  return JS_UNDEFINED;
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
    JS_CFUNC_DEF("appendChild", 1, JsNodeAppendChild),
    JS_CFUNC_DEF("addEventListener", 2, JsNodeAddEventListener),
    JS_CGETSET_DEF("tagName", JsNodeGetTagName, nullptr),
    JS_CGETSET_DEF("textContent", JsNodeGetTextContent, JsNodeSetTextContent),
};

// --- document methods ---

JSValue JsDocumentGetElementById(JSContext *ctx, JSValueConst /*this_val*/,
                                  int argc, JSValueConst *argv) {
  if (argc < 1) {
    return JS_EXCEPTION;
  }
  auto *root = static_cast<Node *>(JS_GetContextOpaque(ctx));
  const char *id = JS_ToCString(ctx, argv[0]);
  Node *found =
      (root != nullptr && id != nullptr) ? root->FindById(id) : nullptr;
  JS_FreeCString(ctx, id);
  return WrapExistingNode(ctx, found);
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
    JS_CFUNC_DEF("createElement", 1, JsDocumentCreateElement),
    JS_CFUNC_DEF("createTextNode", 1, JsDocumentCreateTextNode),
};

} // namespace

struct JsEngine::Impl {
  JSRuntime *rt = nullptr;
  JSContext *ctx = nullptr;

  ~Impl() {
    if (ctx != nullptr) {
      JS_FreeContext(ctx);
    }
    if (rt != nullptr) {
      JS_FreeRuntime(rt);
    }
  }
};

JsEngine::JsEngine(Node &document) : impl_(std::make_unique<Impl>()) {
  impl_->rt = JS_NewRuntime();
  impl_->ctx = JS_NewContext(impl_->rt);

  JS_SetContextOpaque(impl_->ctx, &document);

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

  JSValue global = JS_GetGlobalObject(impl_->ctx);
  JS_SetPropertyStr(impl_->ctx, global, "document", documentObj);
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
