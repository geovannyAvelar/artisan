#pragma once

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

// Depth-first search of `root`'s subtree for an element with this id.
// nullptr if none match - see Node::FindById.
ArtisanNode *ArtisanNodeFindById(ArtisanNode *root, const char *id);

// "" for a text node, e.g. "div"/"button" for an element.
char *ArtisanNodeTagName(ArtisanNode *node);

// See Node::textContent/SetTextContent.
char *ArtisanNodeTextContent(ArtisanNode *node);
void ArtisanNodeSetTextContent(ArtisanNode *node, const char *text);

// nullptr if the attribute isn't set. See Node::GetAttribute.
char *ArtisanNodeGetAttribute(ArtisanNode *node, const char *name);
void ArtisanNodeSetAttribute(ArtisanNode *node, const char *name,
                              const char *value);
void ArtisanNodeRemoveAttribute(ArtisanNode *node, const char *name);

// Registers the handler Click() invokes (see Node::SetOnClick), given as
// a Go closure's cgo.Handle rather than a function pointer - the closure
// itself is invoked through ArtisanGoInvokeClickHandler below. Replaces
// any handler already set on this node, releasing its handle first (via
// ArtisanGoReleaseClickHandler), same as Node::SetOnClick replacing the
// previous std::function would.
void ArtisanNodeSetOnClick(ArtisanNode *node, uintptr_t handle);

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
// from ArtisanNodeSetOnClick's registered handler / its replacement or
// release. Never called if no Go app is configured, since nothing can
// produce a handle in that case.
void ArtisanGoInvokeClickHandler(uintptr_t handle);
void ArtisanGoReleaseClickHandler(uintptr_t handle);

#ifdef __cplusplus
}
#endif
