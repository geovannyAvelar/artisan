#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A plain C ABI over Node (dom_node.h) - the bridge a Go app (which can't
// call C++ directly) uses to drive the same DOM that SetupApp (app.h) and
// JsEngine (js_engine.h) already drive, compiled and linked in the same
// way as the native C++ path: ahead of time, into the same binary. See
// go/artisango for the Go-side wrapper that turns this into an idiomatic
// artisango.Node, and CMakeLists.txt (ARTISAN_APP_GO_SOURCE) for how a
// project's Go app gets compiled into a static archive and linked in.
//
// Every function takes/returns ArtisanNode* - an opaque handle over a
// live artisan::Node* (see node_c_api.cpp) - except FindById, which
// returns nullptr for "no such id", same as Node::FindById itself.
//
// String ownership: every `const char *` passed in is read-only and not
// retained past the call (matches cgo.CString - freed by the Go caller
// right after). Every `char *` returned out is a freshly heap-allocated
// copy the caller must release with ArtisanFreeString - never a pointer
// into Node's own storage, so it stays valid regardless of what the
// caller does to the node afterward.

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ArtisanNode ArtisanNode;

// See Node::type()/NodeType (dom_node.h) - what ArtisanNodeType returns.
#define ARTISAN_NODE_TYPE_ELEMENT 1
#define ARTISAN_NODE_TYPE_TEXT 3

// Depth-first search of `root`'s subtree for an element with this id.
// nullptr if none match - see Node::FindById.
ArtisanNode *ArtisanNodeFindById(ArtisanNode *root, const char *id);

// ARTISAN_NODE_TYPE_ELEMENT or ARTISAN_NODE_TYPE_TEXT above - see
// Node::type().
int ArtisanNodeType(ArtisanNode *node);

// "" for a text node, e.g. "div"/"button" for an element.
char *ArtisanNodeTagName(ArtisanNode *node);

// See Node::textContent/SetTextContent.
char *ArtisanNodeTextContent(ArtisanNode *node);
void ArtisanNodeSetTextContent(ArtisanNode *node, const char *text);

// nullptr if the attribute isn't set. See Node::GetAttribute.
char *ArtisanNodeGetAttribute(ArtisanNode *node, const char *name);
bool ArtisanNodeHasAttribute(ArtisanNode *node, const char *name);
void ArtisanNodeSetAttribute(ArtisanNode *node, const char *name,
                              const char *value);
void ArtisanNodeRemoveAttribute(ArtisanNode *node, const char *name);

// nullptr if there's no parent/no such sibling - see the matching Node
// methods (dom_node.h).
ArtisanNode *ArtisanNodeParentNode(ArtisanNode *node);
ArtisanNode *ArtisanNodeNextSibling(ArtisanNode *node);
ArtisanNode *ArtisanNodePreviousSibling(ArtisanNode *node);

// See Node::children. ArtisanNodeChildAt is undefined behavior if `index`
// isn't `< ArtisanNodeChildCount(node)` - same "caller keeps the count it
// already fetched in range" contract a Go slice index would have, just
// without Go's own bounds check backing it up across the C boundary.
size_t ArtisanNodeChildCount(ArtisanNode *node);
ArtisanNode *ArtisanNodeChildAt(ArtisanNode *node, size_t index);

// Whether `node` itself matches `selector`, and `node`/its nearest
// matching ancestor (inclusive) - same bounded grammar as
// ArtisanQuerySelector below. nullptr if neither `node` nor anything
// above it matches. See css.h's ElementMatches/Closest.
bool ArtisanNodeMatches(ArtisanNode *node, const char *selector);
ArtisanNode *ArtisanNodeClosest(ArtisanNode *node, const char *selector);

// css.h's QuerySelector/QuerySelectorAll - same bounded selector grammar
// documented there (one compound selector, no combinators/comma-lists).
// ArtisanQuerySelector: nullptr if no match. ArtisanQuerySelectorAll:
// `*outCount` matches, in document order, in a malloc'd array the caller
// must release with ArtisanFreeNodeArray - the array itself is owned by
// the caller, but every ArtisanNode* inside it is the usual non-owning
// handle (freeing the array doesn't touch the nodes themselves, same as
// ArtisanFreeString never touches whatever a node's attribute pointed
// into).
ArtisanNode *ArtisanQuerySelector(ArtisanNode *root, const char *selector);
ArtisanNode **ArtisanQuerySelectorAll(ArtisanNode *root, const char *selector,
                                       size_t *outCount);
void ArtisanFreeNodeArray(ArtisanNode **nodes);

// Creates a detached element/text node, exactly like Node::CreateElement/
// CreateText - not yet part of any tree. Ownership lives entirely on the
// C++ side (see node_c_api.cpp's pending-node registry) until it's passed
// to ArtisanNodeAppendChild/InsertBefore below, which transfers it into
// the tree; a node created and never appended is never freed (leaks
// until process exit) - there's no Go-side finalizer to catch that the
// way a garbage-collected JS wrapper would (see js_engine.h's NodeHandle
// for that comparison). Passing the same not-yet-appended ArtisanNode* to
// AppendChild/InsertBefore twice, or passing an ArtisanNode* that isn't a
// currently-pending created node (e.g. one already in the tree, found via
// FindById), is a safe no-op, not an error - there's no exception
// mechanism to report it across a C ABI, so this mirrors the restriction
// the JS binding enforces (there, by throwing) without a way to signal
// failure back to the caller.
ArtisanNode *ArtisanCreateElement(const char *tag);
ArtisanNode *ArtisanCreateTextNode(const char *text);
ArtisanNode *ArtisanNodeAppendChild(ArtisanNode *parent, ArtisanNode *child);
// `before == nullptr` means append at the end, matching
// Node::InsertBefore(child, nullptr).
ArtisanNode *ArtisanNodeInsertBefore(ArtisanNode *parent, ArtisanNode *child,
                                      ArtisanNode *before);

// Detaches `child`/this node from its parent and re-registers it as a
// pending node - the same ownership state ArtisanCreateElement's result
// starts in (see its doc comment: alive but leaked if never re-appended
// via AppendChild/InsertBefore). nullptr if `child` isn't actually a
// child of `parent` (ArtisanNodeRemoveChild), or if `node` has no parent
// (ArtisanNodeRemove) - see Node::RemoveChild/Remove.
ArtisanNode *ArtisanNodeRemoveChild(ArtisanNode *parent, ArtisanNode *child);
ArtisanNode *ArtisanNodeRemove(ArtisanNode *node);

// A new, detached, pending node (same ownership state as
// ArtisanCreateElement's result) - same tag/text/attributes as `node`,
// never event listeners. `deep`: also clones every descendant. See
// Node::CloneNode.
ArtisanNode *ArtisanNodeCloneNode(ArtisanNode *node, bool deep);

// Registers `handle` (a Go closure's cgo.Handle) for `eventType` -
// alongside, not replacing, any other handler already registered for
// that type or any other, matching Node::AddEventListener/real
// addEventListener. `capture`: fires during the capturing phase instead
// of the bubbling phase - see Node::AddEventListener/DispatchEvent. The
// closure itself is invoked through ArtisanGoInvokeHandler below, same
// mechanism ArtisanNodeSetOnClick uses (see it for that comparison).
void ArtisanNodeAddEventListener(ArtisanNode *node, const char *eventType,
                                  uintptr_t handle, bool capture);

// Removes every listener registered for `eventType`/`capture` whose
// handle equals `handle` (the exact value passed to the
// ArtisanNodeAddEventListener call that registered it) - see
// Node::RemoveEventListener. A handle that matches nothing is a safe
// no-op.
void ArtisanNodeRemoveEventListener(ArtisanNode *node, const char *eventType,
                                     uintptr_t handle, bool capture);

// Fires `eventType` at `node` through the usual capturing/target/
// bubbling walk (see Node::DispatchEvent) - every listener registered
// for it, Go or otherwise (see js_engine.h - the same underlying Node
// can have both JS- and Go-registered listeners), runs exactly as it
// would for an internally-fired click/change/input. `bubbles = false`
// skips the ancestor phases; `cancelable = false` makes any listener's
// preventDefault() (JS-side; a Go listener registered here has no way
// to call it - see ArtisanNodeAddEventListener above, whose handler is
// always zero-arg) a no-op. Returns false if the event was cancelable
// and some listener called preventDefault(), true otherwise - the same
// convention real dispatchEvent()'s return value has.
bool ArtisanNodeDispatchEvent(ArtisanNode *node, const char *eventType,
                               bool bubbles, bool cancelable);

// Registers the handler Click() invokes (see Node::SetOnClick), given as
// a Go closure's cgo.Handle rather than a function pointer - the closure
// itself is invoked through ArtisanGoInvokeHandler below. Replaces
// any handler already set on this node, releasing its handle first (via
// ArtisanGoReleaseHandler), same as Node::SetOnClick replacing the
// previous std::function would - unlike ArtisanNodeAddEventListener
// above, which never replaces anything.
void ArtisanNodeSetOnClick(ArtisanNode *node, uintptr_t handle);

// The "class" attribute's space-separated tokens - see
// Node::GetAttribute("class") and js_engine.h's classList doc comment
// (same scope: just the tokens, no CSS selector matching here).
// ArtisanNodeClassListToggle: `hasForce == false` toggles membership;
// `hasForce == true` pins it to `force` instead (real
// classList.toggle(name, force) semantics). Returns the resulting
// membership (true = now present).
void ArtisanNodeClassListAdd(ArtisanNode *node, const char *name);
void ArtisanNodeClassListRemove(ArtisanNode *node, const char *name);
bool ArtisanNodeClassListContains(ArtisanNode *node, const char *name);
bool ArtisanNodeClassListToggle(ArtisanNode *node, const char *name,
                                 bool hasForce, bool force);

// `node`'s inline `style="..."` attribute, one property at a time -
// exactly the five properties a <style> block supports (color/
// backgroundColor/fontWeight/borderColor/borderWidth), same as
// js_engine.h's style doc comment. ArtisanNodeStyleGet: nullptr if
// unset. ArtisanNodeStyleSet: an empty `value` removes the property. See
// css.h's GetInlineStyleProperty/SetInlineStyleProperty.
char *ArtisanNodeStyleGet(ArtisanNode *node, const char *property);
void ArtisanNodeStyleSet(ArtisanNode *node, const char *property,
                          const char *value);

// dataset's data-* convention, method-based rather than true
// dataset.fooBar property syntax (no such syntax to mirror in C either
// way) - "fooBar" <-> "data-foo-bar". ArtisanNodeGetData: nullptr if
// unset.
char *ArtisanNodeGetData(ArtisanNode *node, const char *name);
void ArtisanNodeSetData(ArtisanNode *node, const char *name,
                         const char *value);

// Releases a string ArtisanNodeTagName/TextContent/GetAttribute returned.
void ArtisanFreeString(char *str);

// The Go app's entry point: called once whenever a page loads (first at
// startup, then again on every navigation - see main.cpp's navigate()),
// the same way SetupApp(Node&) and the embedded script both are.
// Implemented by the Go archive (see go/artisango's main-package
// template) when a project configures ARTISAN_APP_GO_SOURCE, or by a
// generated no-op stub otherwise (see CMakeLists.txt) - either way,
// main.cpp calls it unconditionally, the same way it always calls
// GetAppScript() whether or not a script was configured.
void ArtisanSetupApp(ArtisanNode *document);

// Implemented by the Go archive (//export'ed from go/artisango), called
// from ArtisanNodeSetOnClick's/ArtisanNodeAddEventListener's registered
// handlers / their replacement or release. Never called if no Go app is
// configured, since nothing can produce a handle in that case.
void ArtisanGoInvokeHandler(uintptr_t handle);
void ArtisanGoReleaseHandler(uintptr_t handle);

#ifdef __cplusplus
}
#endif
