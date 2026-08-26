// Package artisango is the Go counterpart to SetupApp (C++) and the
// embedded script (JS): a thin, idiomatic wrapper over node_c_api.h, the
// C ABI artisan exposes so a Go app - compiled ahead of time into a
// static archive and linked into the same binary, see CMakeLists.txt's
// ARTISAN_APP_GO_SOURCE - can drive the same mutable DOM (dom_node.h)
// SetupApp and script both drive.
//
// A project's own Go app is a package main that imports artisango and
// exports ArtisanSetupApp - see the template artisan-cli's `component`-
// style scaffolding writes into a new project's goapp/main.go:
//
//	package main
//
//	import "C"
//	import (
//		"unsafe"
//
//		"artisango"
//	)
//
//	func SetupApp(doc artisango.Node) {
//		button := doc.FindById("my-button")
//		if button != nil {
//			button.SetOnClick(func() {
//				// ...
//			})
//		}
//	}
//
//	//export ArtisanSetupApp
//	func ArtisanSetupApp(doc unsafe.Pointer) {
//		SetupApp(artisango.WrapNode(doc))
//	}
//
//	func main() {}
package artisango

/*
#cgo CFLAGS: -I${SRCDIR}/../../include
#include "node_c_api.h"
#include <stdlib.h>
*/
import "C"

import (
	"runtime/cgo"
	"unsafe"
)

// Node wraps a live ArtisanNode* - never construct one directly; get one
// from WrapNode (the document, at the top of ArtisanSetupApp) or from
// another Node's FindById.
type Node struct {
	ptr *C.ArtisanNode
}

// WrapNode turns the raw pointer ArtisanSetupApp receives into a Node.
func WrapNode(ptr unsafe.Pointer) Node {
	return Node{ptr: (*C.ArtisanNode)(ptr)}
}

// FindById is a depth-first search of this subtree (including the node
// itself) for an element whose "id" attribute equals id. nil if none
// match.
func (n Node) FindById(id string) *Node {
	cid := C.CString(id)
	defer C.free(unsafe.Pointer(cid))
	return wrapOrNil(C.ArtisanNodeFindById(n.ptr, cid))
}

// TagName is "" for a text node, e.g. "div"/"button" for an element.
func (n Node) TagName() string {
	cstr := C.ArtisanNodeTagName(n.ptr)
	defer C.ArtisanFreeString(cstr)
	return C.GoString(cstr)
}

// TextContent is this node's own text (text node) or the concatenation
// of every descendant text node's content, depth-first (element).
func (n Node) TextContent() string {
	cstr := C.ArtisanNodeTextContent(n.ptr)
	defer C.ArtisanFreeString(cstr)
	return C.GoString(cstr)
}

// SetTextContent replaces a text node's own text, or - matching real DOM
// `.textContent =` semantics - discards an element's existing children
// and replaces them with a single new text node.
func (n Node) SetTextContent(text string) {
	ctext := C.CString(text)
	defer C.free(unsafe.Pointer(ctext))
	C.ArtisanNodeSetTextContent(n.ptr, ctext)
}

// GetAttribute reports the named attribute's value and whether it's set
// at all (an empty string and an unset attribute both report ok - check
// ok, not the string, to tell them apart).
func (n Node) GetAttribute(name string) (value string, ok bool) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	cvalue := C.ArtisanNodeGetAttribute(n.ptr, cname)
	if cvalue == nil {
		return "", false
	}
	defer C.ArtisanFreeString(cvalue)
	return C.GoString(cvalue), true
}

func (n Node) SetAttribute(name, value string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	cvalue := C.CString(value)
	defer C.free(unsafe.Pointer(cvalue))
	C.ArtisanNodeSetAttribute(n.ptr, cname, cvalue)
}

func (n Node) RemoveAttribute(name string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	C.ArtisanNodeRemoveAttribute(n.ptr, cname)
}

// HasAttribute reports whether the named attribute is set at all,
// regardless of its value (including "").
func (n Node) HasAttribute(name string) bool {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return bool(C.ArtisanNodeHasAttribute(n.ptr, cname))
}

// ParentNode, NextSibling, and PreviousSibling are nil if there's no
// parent/no such sibling - see the matching Node methods (dom_node.h).
func (n Node) ParentNode() *Node {
	return wrapOrNil(C.ArtisanNodeParentNode(n.ptr))
}

func (n Node) NextSibling() *Node {
	return wrapOrNil(C.ArtisanNodeNextSibling(n.ptr))
}

func (n Node) PreviousSibling() *Node {
	return wrapOrNil(C.ArtisanNodePreviousSibling(n.ptr))
}

// Children is a snapshot of this node's children at call time, not a
// live view - the same simplification document.querySelectorAll's Go/JS
// counterparts already make (see css.h's QuerySelector doc comment).
func (n Node) Children() []Node {
	count := int(C.ArtisanNodeChildCount(n.ptr))
	children := make([]Node, count)
	for i := 0; i < count; i++ {
		children[i] = Node{ptr: C.ArtisanNodeChildAt(n.ptr, C.size_t(i))}
	}
	return children
}

// QuerySelector/QuerySelectorAll search this node's subtree (not
// including the node itself) for element(s) matching selector - the same
// bounded grammar documented on css.h's Selector (one compound selector,
// no combinators/comma-lists). QuerySelector is nil, and
// QuerySelectorAll is an empty (never nil) slice, if nothing matches.
func (n Node) QuerySelector(selector string) *Node {
	cselector := C.CString(selector)
	defer C.free(unsafe.Pointer(cselector))
	return wrapOrNil(C.ArtisanQuerySelector(n.ptr, cselector))
}

func (n Node) QuerySelectorAll(selector string) []Node {
	cselector := C.CString(selector)
	defer C.free(unsafe.Pointer(cselector))
	var count C.size_t
	arr := C.ArtisanQuerySelectorAll(n.ptr, cselector, &count)
	if arr == nil {
		return []Node{}
	}
	defer C.ArtisanFreeNodeArray(arr)
	ptrs := unsafe.Slice(arr, int(count))
	found := make([]Node, int(count))
	for i, ptr := range ptrs {
		found[i] = Node{ptr: ptr}
	}
	return found
}

// wrapOrNil turns a possibly-nullptr ArtisanNode* into a possibly-nil
// *Node - the shared "found nothing" shape FindById/ParentNode/
// NextSibling/PreviousSibling/QuerySelector all return.
func wrapOrNil(ptr *C.ArtisanNode) *Node {
	if ptr == nil {
		return nil
	}
	return &Node{ptr: ptr}
}

// SetOnClick registers the handler Click() invokes on the C++ side -
// typically used for a <button>. Replaces (and releases) any handler
// already set on this node.
func (n Node) SetOnClick(fn func()) {
	handle := cgo.NewHandle(fn)
	C.ArtisanNodeSetOnClick(n.ptr, C.uintptr_t(handle))
}

// AddEventListener registers fn for eventType, alongside (not replacing)
// any other handler already registered for that type or any other -
// unlike SetOnClick above, matching real addEventListener. Any type
// string works, but only "click", "change" (checkbox/radio), and
// "input" (text fields) actually fire today (see main.cpp).
func (n Node) AddEventListener(eventType string, fn func()) {
	ceventType := C.CString(eventType)
	defer C.free(unsafe.Pointer(ceventType))
	handle := cgo.NewHandle(fn)
	C.ArtisanNodeAddEventListener(n.ptr, ceventType, C.uintptr_t(handle))
}

// CreateElement/CreateTextNode create a detached element/text node - not
// yet part of any tree. Append it somewhere (AppendChild/InsertBefore
// below) before this Node value goes out of scope: the underlying C++
// object is only freed once actually attached, or never, if it's
// abandoned instead - there's no Go-side finalizer to catch that the way
// a garbage-collected JS wrapper would (see ArtisanCreateElement's doc
// comment in node_c_api.h).
func CreateElement(tag string) Node {
	ctag := C.CString(tag)
	defer C.free(unsafe.Pointer(ctag))
	return Node{ptr: C.ArtisanCreateElement(ctag)}
}

func CreateTextNode(text string) Node {
	ctext := C.CString(text)
	defer C.free(unsafe.Pointer(ctext))
	return Node{ptr: C.ArtisanCreateTextNode(ctext)}
}

// AppendChild takes ownership of a node created via CreateElement/
// CreateTextNode, appending it after this node's existing children.
// Passing a node that isn't currently a pending, unattached
// CreateElement/CreateTextNode result - one already attached elsewhere,
// say - is a safe no-op (re-parenting isn't supported, same restriction
// the JS binding enforces, there by throwing; there's no exception
// mechanism to report that here). Returns child back, for chaining.
func (n Node) AppendChild(child Node) Node {
	C.ArtisanNodeAppendChild(n.ptr, child.ptr)
	return child
}

// InsertBefore is AppendChild's more general form: inserts child
// directly before the given sibling instead of at the end. before == nil
// means append at the end, matching real DOM insertBefore(node, null).
func (n Node) InsertBefore(child Node, before *Node) Node {
	var cbefore *C.ArtisanNode
	if before != nil {
		cbefore = before.ptr
	}
	C.ArtisanNodeInsertBefore(n.ptr, child.ptr, cbefore)
	return child
}

// ArtisanGoInvokeHandler is called from C++ (node_c_api.cpp's
// GoCallback) when a node registered via SetOnClick or AddEventListener
// fires - not meant to be called directly from Go app code.
//
//export ArtisanGoInvokeHandler
func ArtisanGoInvokeHandler(handle C.uintptr_t) {
	fn := cgo.Handle(handle).Value().(func())
	fn()
}

// ArtisanGoReleaseHandler is called from C++ when a handler is replaced
// or its node destroyed, to release the handle SetOnClick/
// AddEventListener created - not meant to be called directly from Go app
// code.
//
//export ArtisanGoReleaseHandler
func ArtisanGoReleaseHandler(handle C.uintptr_t) {
	cgo.Handle(handle).Delete()
}
